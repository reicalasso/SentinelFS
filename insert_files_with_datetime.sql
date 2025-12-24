-- Yeni bir peer ekliyoruz (ilk kez ağa katılıyor)
INSERT INTO peers (
    peer_id,
    name,
    address,
    port,
    public_key,
    status_id,
    last_seen,
    latency
)
VALUES (
    'TST-001',
    'Main Node',
    '244.178.44.111',
    9000,
    NULL,              -- Henüz key exchange yapılmadı
    2,                 -- 2 = ONLINE
    strftime('%s','now'),
    23                 -- Latency henüz ölçülmedi
);




-- Sistemde hangi peer durumları var?
SELECT id, name
FROM status_types
ORDER BY id;



-- Peer bağlantı kurdu, online oldu
UPDATE peers
SET
    status_id = 2,                    -- ONLINE
    latency   = 23,                   -- ms
    last_seen = strftime('%s','now')
WHERE peer_id = 'node-001';

