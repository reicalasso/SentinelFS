#!/usr/bin/env python3
"""
SentinelFS Relay Server - Lightweight Introducer/Relay for WAN Discovery

This is a lightweight relay server that enables:
1. Global peer discovery across networks
2. NAT traversal via relay when direct connections fail
3. Session code based peer matching

Protocol:
- Peers register with their peer ID and session code
- Peers with matching session codes are introduced to each other
- Data can be relayed between peers when direct connection fails

Run:
    python3 relay_server.py --port 9000
    
Or with Docker:
    docker run -p 9000:9000 sentinelfs-relay
"""

import asyncio
import argparse
import logging
import struct
import hashlib
import time
import json
from dataclasses import dataclass, field
from typing import Dict, Optional, Set, Tuple
from enum import IntEnum

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(name)s: %(message)s'
)
logger = logging.getLogger('RelayServer')


class MessageType(IntEnum):
    """Relay protocol message types"""
    REGISTER = 0x01
    REGISTER_ACK = 0x02
    PEER_LIST = 0x03
    CONNECT = 0x04
    CONNECT_ACK = 0x05
    DATA = 0x06
    HEARTBEAT = 0x07
    DISCONNECT = 0x08
    ERROR = 0xFF
    # Extended messages for NAT traversal
    PUNCH_REQUEST = 0x10      # Request hole punch coordination
    PUNCH_SYNC = 0x11         # Synchronize punch timing
    EXTERNAL_ADDR = 0x12      # Report external address


@dataclass
class PeerInfo:
    """Information about a connected peer"""
    peer_id: str
    session_code: str
    writer: asyncio.StreamWriter
    public_ip: str
    public_port: int
    private_ip: str = ""
    private_port: int = 0
    connected_at: float = field(default_factory=time.time)
    last_heartbeat: float = field(default_factory=time.time)
    nat_type: str = "unknown"  # symmetric, cone, open
    relayed_bytes: int = 0


class RelayServer:
    """
    Lightweight relay server for SentinelFS peer discovery and NAT traversal
    """
    
    def __init__(self, host: str = "0.0.0.0", port: int = 9000):
        self.host = host
        self.port = port
        
        # Connected peers: peer_id -> PeerInfo
        self.peers: Dict[str, PeerInfo] = {}
        
        # Session groups: session_code -> set of peer_ids
        self.sessions: Dict[str, Set[str]] = {}
        
        # Lock for thread-safe access
        self.lock = asyncio.Lock()
        
        # Stats
        self.stats = {
            'total_connections': 0,
            'total_bytes_relayed': 0,
            'total_introductions': 0,
            'active_sessions': 0,
        }
        
        # Cleanup task
        self._cleanup_task: Optional[asyncio.Task] = None
    
    async def start(self):
        """Start the relay server"""
        server = await asyncio.start_server(
            self._handle_client,
            self.host,
            self.port
        )
        
        addr = server.sockets[0].getsockname()
        logger.info(f"Relay server listening on {addr[0]}:{addr[1]}")
        
        # Start cleanup task
        self._cleanup_task = asyncio.create_task(self._cleanup_loop())
        
        async with server:
            await server.serve_forever()
    
    async def _handle_client(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter):
        """Handle a client connection"""
        addr = writer.get_extra_info('peername')
        client_ip = addr[0] if addr else 'unknown'
        client_port = addr[1] if addr else 0
        
        logger.info(f"New connection from {client_ip}:{client_port}")
        self.stats['total_connections'] += 1
        
        peer_id: Optional[str] = None
        
        try:
            while True:
                # Read message header: type (1 byte) + length (4 bytes)
                header = await reader.readexactly(5)
                msg_type = MessageType(header[0])
                length = struct.unpack('!I', header[1:5])[0]
                
                # Sanity check
                if length > 10 * 1024 * 1024:  # Max 10MB
                    logger.warning(f"Invalid message length from {client_ip}: {length}")
                    break
                
                # Read payload
                payload = await reader.readexactly(length) if length > 0 else b''
                
                # Handle message
                response = await self._handle_message(
                    msg_type, payload, reader, writer, client_ip, client_port, peer_id
                )
                
                if response:
                    if isinstance(response, tuple):
                        # Response includes peer_id update
                        peer_id = response[1]
                        response = response[0]
                    
                    if response:
                        writer.write(response)
                        await writer.drain()
                
        except asyncio.IncompleteReadError:
            logger.debug(f"Client {client_ip} disconnected")
        except Exception as e:
            logger.error(f"Error handling client {client_ip}: {e}")
        finally:
            # Cleanup on disconnect
            if peer_id:
                await self._remove_peer(peer_id)
            writer.close()
            await writer.wait_closed()
    
    async def _handle_message(
        self, 
        msg_type: MessageType, 
        payload: bytes,
        reader: asyncio.StreamReader,
        writer: asyncio.StreamWriter,
        client_ip: str,
        client_port: int,
        current_peer_id: Optional[str]
    ) -> Optional[bytes | Tuple[bytes, str]]:
        """Handle a protocol message"""
        
        if msg_type == MessageType.REGISTER:
            return await self._handle_register(payload, writer, client_ip, client_port)
        
        elif msg_type == MessageType.PEER_LIST:
            if current_peer_id:
                return await self._handle_peer_list(current_peer_id)
        
        elif msg_type == MessageType.DATA:
            if current_peer_id:
                await self._handle_relay_data(current_peer_id, payload)
        
        elif msg_type == MessageType.HEARTBEAT:
            if current_peer_id:
                await self._update_heartbeat(current_peer_id)
                return self._make_message(MessageType.HEARTBEAT, b'')
        
        elif msg_type == MessageType.CONNECT:
            if current_peer_id:
                return await self._handle_connect_request(current_peer_id, payload)
        
        elif msg_type == MessageType.PUNCH_REQUEST:
            if current_peer_id:
                return await self._handle_punch_request(current_peer_id, payload)
        
        elif msg_type == MessageType.EXTERNAL_ADDR:
            if current_peer_id:
                await self._update_external_addr(current_peer_id, payload)
        
        return None
    
    async def _handle_register(
        self, 
        payload: bytes, 
        writer: asyncio.StreamWriter,
        client_ip: str,
        client_port: int
    ) -> Tuple[bytes, str]:
        """Handle peer registration"""
        try:
            # Parse: peer_id_len (1) + peer_id + session_code_len (1) + session_code
            offset = 0
            peer_id_len = payload[offset]
            offset += 1
            peer_id = payload[offset:offset + peer_id_len].decode('utf-8')
            offset += peer_id_len
            
            session_code_len = payload[offset]
            offset += 1
            session_code = payload[offset:offset + session_code_len].decode('utf-8')
            
            # Optional: private address info
            private_ip = ""
            private_port = 0
            if offset + session_code_len < len(payload):
                offset += session_code_len
                if len(payload) > offset:
                    priv_ip_len = payload[offset]
                    offset += 1
                    private_ip = payload[offset:offset + priv_ip_len].decode('utf-8')
                    offset += priv_ip_len
                    if len(payload) >= offset + 2:
                        private_port = struct.unpack('!H', payload[offset:offset + 2])[0]
            
            async with self.lock:
                # Create peer info
                peer_info = PeerInfo(
                    peer_id=peer_id,
                    session_code=session_code,
                    writer=writer,
                    public_ip=client_ip,
                    public_port=client_port,
                    private_ip=private_ip,
                    private_port=private_port
                )
                
                # Remove old registration if exists
                if peer_id in self.peers:
                    old_session = self.peers[peer_id].session_code
                    if old_session in self.sessions:
                        self.sessions[old_session].discard(peer_id)
                
                # Register peer
                self.peers[peer_id] = peer_info
                
                # Add to session group
                if session_code not in self.sessions:
                    self.sessions[session_code] = set()
                self.sessions[session_code].add(peer_id)
                
                self.stats['active_sessions'] = len(self.sessions)
            
            logger.info(f"Registered peer {peer_id[:8]}... in session {session_code[:8]}... from {client_ip}:{client_port}")
            
            # Send ACK
            ack = self._make_message(MessageType.REGISTER_ACK, b'OK')
            
            # Notify other peers in session about new peer
            await self._notify_session_peers(session_code, peer_id, peer_info)
            
            return (ack, peer_id)
            
        except Exception as e:
            logger.error(f"Registration error: {e}")
            return (self._make_message(MessageType.ERROR, str(e).encode()), "")
    
    async def _handle_peer_list(self, requester_id: str) -> bytes:
        """Return list of peers in same session"""
        async with self.lock:
            if requester_id not in self.peers:
                return self._make_message(MessageType.ERROR, b'Not registered')
            
            session_code = self.peers[requester_id].session_code
            session_peers = self.sessions.get(session_code, set())
            
            # Build peer list (exclude requester)
            peers_data = []
            for pid in session_peers:
                if pid != requester_id and pid in self.peers:
                    peer = self.peers[pid]
                    peers_data.append(peer)
            
            # Format: count (1) + [peer_id_len (1) + peer_id + ip_len (1) + ip + port (2)] * count
            payload = bytearray([len(peers_data)])
            
            for peer in peers_data:
                peer_id_bytes = peer.peer_id.encode()
                ip_bytes = peer.public_ip.encode()
                
                payload.append(len(peer_id_bytes))
                payload.extend(peer_id_bytes)
                payload.append(len(ip_bytes))
                payload.extend(ip_bytes)
                payload.extend(struct.pack('!H', peer.public_port))
            
            return self._make_message(MessageType.PEER_LIST, bytes(payload))
    
    async def _handle_relay_data(self, from_peer_id: str, payload: bytes):
        """Relay data between peers"""
        try:
            # Parse: target_peer_id_len (1) + target_peer_id + data
            target_len = payload[0]
            target_id = payload[1:1 + target_len].decode('utf-8')
            data = payload[1 + target_len:]
            
            async with self.lock:
                if target_id not in self.peers:
                    logger.debug(f"Target peer {target_id[:8]}... not found for relay")
                    return
                
                target_peer = self.peers[target_id]
                
                # Verify same session
                if from_peer_id in self.peers:
                    if self.peers[from_peer_id].session_code != target_peer.session_code:
                        logger.warning(f"Session mismatch for relay: {from_peer_id[:8]} -> {target_id[:8]}")
                        return
            
            # Build relay message: from_peer_id_len (1) + from_peer_id + data
            from_id_bytes = from_peer_id.encode()
            relay_payload = bytearray([len(from_id_bytes)])
            relay_payload.extend(from_id_bytes)
            relay_payload.extend(data)
            
            msg = self._make_message(MessageType.DATA, bytes(relay_payload))
            
            try:
                target_peer.writer.write(msg)
                await target_peer.writer.drain()
                
                # Update stats
                self.stats['total_bytes_relayed'] += len(data)
                async with self.lock:
                    if from_peer_id in self.peers:
                        self.peers[from_peer_id].relayed_bytes += len(data)
                
                logger.debug(f"Relayed {len(data)} bytes: {from_peer_id[:8]} -> {target_id[:8]}")
                
            except Exception as e:
                logger.error(f"Failed to relay to {target_id[:8]}: {e}")
                
        except Exception as e:
            logger.error(f"Relay data error: {e}")
    
    async def _handle_connect_request(self, from_peer_id: str, payload: bytes) -> bytes:
        """Handle direct connection request (for NAT hole punching coordination)"""
        try:
            target_len = payload[0]
            target_id = payload[1:1 + target_len].decode('utf-8')
            
            async with self.lock:
                if target_id not in self.peers:
                    return self._make_message(MessageType.ERROR, b'Peer not found')
                
                target_peer = self.peers[target_id]
                from_peer = self.peers.get(from_peer_id)
                
                if not from_peer:
                    return self._make_message(MessageType.ERROR, b'Not registered')
                
                if from_peer.session_code != target_peer.session_code:
                    return self._make_message(MessageType.ERROR, b'Session mismatch')
            
            # Send connection info for both peers
            # This enables NAT traversal by sharing endpoint info
            
            # Notify target peer about connection request
            connect_payload = bytearray([len(from_peer_id)])
            connect_payload.extend(from_peer_id.encode())
            connect_payload.append(len(from_peer.public_ip))
            connect_payload.extend(from_peer.public_ip.encode())
            connect_payload.extend(struct.pack('!H', from_peer.public_port))
            
            # Add private endpoint if available
            if from_peer.private_ip:
                connect_payload.append(len(from_peer.private_ip))
                connect_payload.extend(from_peer.private_ip.encode())
                connect_payload.extend(struct.pack('!H', from_peer.private_port))
            
            notify_msg = self._make_message(MessageType.CONNECT, bytes(connect_payload))
            
            try:
                target_peer.writer.write(notify_msg)
                await target_peer.writer.drain()
            except:
                pass
            
            # Return target's info to requester
            ack_payload = bytearray([len(target_id)])
            ack_payload.extend(target_id.encode())
            ack_payload.append(len(target_peer.public_ip))
            ack_payload.extend(target_peer.public_ip.encode())
            ack_payload.extend(struct.pack('!H', target_peer.public_port))
            
            if target_peer.private_ip:
                ack_payload.append(len(target_peer.private_ip))
                ack_payload.extend(target_peer.private_ip.encode())
                ack_payload.extend(struct.pack('!H', target_peer.private_port))
            
            self.stats['total_introductions'] += 1
            
            return self._make_message(MessageType.CONNECT_ACK, bytes(ack_payload))
            
        except Exception as e:
            logger.error(f"Connect request error: {e}")
            return self._make_message(MessageType.ERROR, str(e).encode())
    
    async def _handle_punch_request(self, from_peer_id: str, payload: bytes) -> bytes:
        """Coordinate NAT hole punching between peers"""
        try:
            target_len = payload[0]
            target_id = payload[1:1 + target_len].decode('utf-8')
            
            async with self.lock:
                if target_id not in self.peers:
                    return self._make_message(MessageType.ERROR, b'Peer not found')
                
                target_peer = self.peers[target_id]
                from_peer = self.peers.get(from_peer_id)
                
                if not from_peer:
                    return self._make_message(MessageType.ERROR, b'Not registered')
            
            # Send synchronized punch timing to both peers
            # Both peers should start punching at the same time
            punch_time = int(time.time() * 1000) + 500  # Start in 500ms
            
            sync_payload = struct.pack('!Q', punch_time)
            sync_payload += bytes([len(target_peer.public_ip)])
            sync_payload += target_peer.public_ip.encode()
            sync_payload += struct.pack('!H', target_peer.public_port)
            
            # Notify target
            target_sync = struct.pack('!Q', punch_time)
            target_sync += bytes([len(from_peer.public_ip)])
            target_sync += from_peer.public_ip.encode()
            target_sync += struct.pack('!H', from_peer.public_port)
            
            try:
                target_msg = self._make_message(MessageType.PUNCH_SYNC, target_sync)
                target_peer.writer.write(target_msg)
                await target_peer.writer.drain()
            except:
                pass
            
            return self._make_message(MessageType.PUNCH_SYNC, sync_payload)
            
        except Exception as e:
            logger.error(f"Punch request error: {e}")
            return self._make_message(MessageType.ERROR, str(e).encode())
    
    async def _update_heartbeat(self, peer_id: str):
        """Update peer heartbeat timestamp"""
        async with self.lock:
            if peer_id in self.peers:
                self.peers[peer_id].last_heartbeat = time.time()
    
    async def _update_external_addr(self, peer_id: str, payload: bytes):
        """Update peer's reported external address"""
        try:
            ip_len = payload[0]
            external_ip = payload[1:1 + ip_len].decode('utf-8')
            external_port = struct.unpack('!H', payload[1 + ip_len:3 + ip_len])[0]
            
            async with self.lock:
                if peer_id in self.peers:
                    peer = self.peers[peer_id]
                    # If external differs from what we see, peer is likely behind NAT
                    if external_ip != peer.public_ip:
                        peer.nat_type = "symmetric"
                    else:
                        peer.nat_type = "cone"
                        
        except Exception as e:
            logger.error(f"External addr update error: {e}")
    
    async def _notify_session_peers(self, session_code: str, new_peer_id: str, new_peer: PeerInfo):
        """Notify existing session peers about new peer"""
        async with self.lock:
            session_peers = self.sessions.get(session_code, set())
            
            for peer_id in session_peers:
                if peer_id != new_peer_id and peer_id in self.peers:
                    peer = self.peers[peer_id]
                    
                    # Send peer notification
                    payload = bytearray([1])  # Count = 1
                    peer_id_bytes = new_peer_id.encode()
                    ip_bytes = new_peer.public_ip.encode()
                    
                    payload.append(len(peer_id_bytes))
                    payload.extend(peer_id_bytes)
                    payload.append(len(ip_bytes))
                    payload.extend(ip_bytes)
                    payload.extend(struct.pack('!H', new_peer.public_port))
                    
                    msg = self._make_message(MessageType.PEER_LIST, bytes(payload))
                    
                    try:
                        peer.writer.write(msg)
                        await peer.writer.drain()
                    except:
                        pass
    
    async def _remove_peer(self, peer_id: str):
        """Remove peer and notify session members"""
        async with self.lock:
            if peer_id not in self.peers:
                return
            
            peer = self.peers[peer_id]
            session_code = peer.session_code
            
            # Remove from session
            if session_code in self.sessions:
                self.sessions[session_code].discard(peer_id)
                if not self.sessions[session_code]:
                    del self.sessions[session_code]
            
            # Notify session peers
            for other_id in self.sessions.get(session_code, set()):
                if other_id in self.peers:
                    other_peer = self.peers[other_id]
                    
                    # Send disconnect notification
                    payload = bytes([len(peer_id)]) + peer_id.encode()
                    msg = self._make_message(MessageType.DISCONNECT, payload)
                    
                    try:
                        other_peer.writer.write(msg)
                        await other_peer.writer.drain()
                    except:
                        pass
            
            del self.peers[peer_id]
            self.stats['active_sessions'] = len(self.sessions)
            
            logger.info(f"Peer {peer_id[:8]}... disconnected")
    
    async def _cleanup_loop(self):
        """Periodic cleanup of stale connections"""
        while True:
            await asyncio.sleep(60)  # Run every minute
            
            now = time.time()
            stale_peers = []
            
            async with self.lock:
                for peer_id, peer in self.peers.items():
                    # 90 seconds without heartbeat = stale
                    if now - peer.last_heartbeat > 90:
                        stale_peers.append(peer_id)
            
            for peer_id in stale_peers:
                logger.info(f"Cleaning up stale peer: {peer_id[:8]}...")
                await self._remove_peer(peer_id)
    
    def _make_message(self, msg_type: MessageType, payload: bytes) -> bytes:
        """Create a protocol message"""
        header = bytes([msg_type]) + struct.pack('!I', len(payload))
        return header + payload
    
    def get_stats(self) -> dict:
        """Get server statistics"""
        return {
            **self.stats,
            'active_peers': len(self.peers),
            'active_sessions': len(self.sessions),
        }


async def main():
    parser = argparse.ArgumentParser(description='SentinelFS Relay Server')
    parser.add_argument('--host', default='0.0.0.0', help='Bind address')
    parser.add_argument('--port', type=int, default=9000, help='Listen port')
    parser.add_argument('--debug', action='store_true', help='Enable debug logging')
    
    args = parser.parse_args()
    
    if args.debug:
        logging.getLogger().setLevel(logging.DEBUG)
    
    server = RelayServer(host=args.host, port=args.port)
    
    try:
        await server.start()
    except KeyboardInterrupt:
        logger.info("Shutting down...")


if __name__ == '__main__':
    asyncio.run(main())
