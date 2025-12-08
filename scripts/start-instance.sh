#!/bin/bash
#
# SentinelFS Multi-Instance Starter
# Farklı portlarda birden fazla daemon instance'ı başlatır
#
# Kullanım:
#   ./start-instance.sh [instance_id]
#   ./start-instance.sh 1        # Instance 1: TCP=8081, UDP=9991
#   ./start-instance.sh 2        # Instance 2: TCP=8082, UDP=9992
#   ./start-instance.sh stop     # Tüm instance'ları durdur
#   ./start-instance.sh status   # Instance durumlarını göster
#   ./start-instance.sh gencode  # Session code oluştur
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"
DAEMON_BIN="$BUILD_DIR/app/daemon/sentinel_daemon"

# Renk kodları
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Varsayılan portlar (instance 0 = ana daemon)
BASE_TCP_PORT=8080
BASE_UDP_PORT=9999
BASE_METRICS_PORT=9100

print_usage() {
    echo -e "${BLUE}SentinelFS Multi-Instance Starter${NC}"
    echo ""
    echo "Kullanım:"
    echo "  $0 <instance_id>    - Belirtilen instance'ı başlat (1-9)"
    echo "  $0 stop [id]        - Tüm veya belirtilen instance'ı durdur"
    echo "  $0 status           - Çalışan instance'ları göster"
    echo "  $0 list             - Port yapılandırmasını göster"
    echo "  $0 codes            - Tüm session code'ları göster"
    echo "  $0 gencode <id>     - Belirtilen instance için yeni session code oluştur"
    echo ""
    echo "Örnekler:"
    echo "  $0 1                # Instance 1 başlat (session code otomatik oluşturulur)"
    echo "  $0 2                # Instance 2 başlat"
    echo "  $0 stop 1           # Instance 1'i durdur"
    echo "  $0 stop             # Tüm instance'ları durdur"
    echo "  $0 codes            # Tüm session code'ları listele"
    echo "  $0 gencode 1        # Instance 1 için yeni session code oluştur"
}

get_ports() {
    local id=$1
    TCP_PORT=$((BASE_TCP_PORT + id))
    UDP_PORT=$((BASE_UDP_PORT - 8 + id))  # 9991, 9992, ...
    METRICS_PORT=$((BASE_METRICS_PORT + id))
}

get_paths() {
    local id=$1
    WATCH_DIR="$HOME/SentinelFS_$id"
    DATA_DIR="$HOME/.local/share/sentinelfs_$id"
    DB_PATH="$DATA_DIR/sentinel.db"
    IPC_SOCKET="/run/user/$(id -u)/sentinelfs/sentinel_daemon_$id.sock"
    SESSION_CODE_FILE="$DATA_DIR/session_code"
}

generate_session_code() {
    # 6 haneli alfanumerik session code oluştur
    local code=$(cat /dev/urandom | tr -dc 'A-Z0-9' | fold -w 6 | head -n 1)
    echo "$code"
}

get_or_create_session_code() {
    local id=$1
    get_paths $id
    
    mkdir -p "$DATA_DIR"
    
    if [[ -f "$SESSION_CODE_FILE" ]]; then
        cat "$SESSION_CODE_FILE"
    else
        local code=$(generate_session_code)
        echo "$code" > "$SESSION_CODE_FILE"
        chmod 600 "$SESSION_CODE_FILE"
        echo "$code"
    fi
}

check_daemon_binary() {
    if [[ ! -x "$DAEMON_BIN" ]]; then
        echo -e "${RED}Hata: Daemon binary bulunamadı: $DAEMON_BIN${NC}"
        echo "Önce projeyi derleyin: cd $PROJECT_ROOT && cmake --build build"
        exit 1
    fi
}

start_instance() {
    local id=$1
    
    if [[ ! "$id" =~ ^[1-9]$ ]]; then
        echo -e "${RED}Hata: Instance ID 1-9 arasında olmalı${NC}"
        exit 1
    fi
    
    check_daemon_binary
    get_ports $id
    get_paths $id
    
    # Watch dizinini oluştur
    mkdir -p "$WATCH_DIR"
    mkdir -p "$DATA_DIR"
    
    # Session code al veya oluştur
    local SESSION_CODE=$(get_or_create_session_code $id)
    
    # Zaten çalışıyor mu kontrol et
    if pgrep -f "sentinel_daemon.*--port $TCP_PORT" > /dev/null 2>&1; then
        echo -e "${YELLOW}Instance $id zaten çalışıyor (TCP: $TCP_PORT)${NC}"
        echo -e "  Session Code: ${BLUE}$SESSION_CODE${NC}"
        return 0
    fi
    
    echo -e "${GREEN}Instance $id başlatılıyor...${NC}"
    echo "  TCP Port:     $TCP_PORT"
    echo "  UDP Port:     $UDP_PORT"
    echo "  Metrics Port: $METRICS_PORT"
    echo "  Watch Dir:    $WATCH_DIR"
    echo "  Database:     $DB_PATH"
    echo -e "  Session Code: ${BLUE}$SESSION_CODE${NC}"
    
    # Daemon'ı başlat
    cd "$BUILD_DIR"
    SENTINEL_DB_PATH="$DB_PATH" \
    SENTINEL_IPC_SOCKET="$IPC_SOCKET" \
    ./app/daemon/sentinel_daemon \
        --port $TCP_PORT \
        --discovery $UDP_PORT \
        --watch "$WATCH_DIR" \
        --session-code "$SESSION_CODE" \
        > "$DATA_DIR/daemon.log" 2>&1 &
    
    local pid=$!
    echo $pid > "$DATA_DIR/daemon.pid"
    
    sleep 1
    
    if kill -0 $pid 2>/dev/null; then
        echo -e "${GREEN}✓ Instance $id başlatıldı (PID: $pid)${NC}"
    else
        echo -e "${RED}✗ Instance $id başlatılamadı. Log: $DATA_DIR/daemon.log${NC}"
        tail -20 "$DATA_DIR/daemon.log" 2>/dev/null
        exit 1
    fi
}

stop_instance() {
    local id=$1
    
    if [[ -z "$id" ]]; then
        # Tüm instance'ları durdur
        echo -e "${YELLOW}Tüm SentinelFS instance'ları durduruluyor...${NC}"
        pkill -f "sentinel_daemon" 2>/dev/null || true
        
        # PID dosyalarını temizle
        for i in {1..9}; do
            get_paths $i
            rm -f "$DATA_DIR/daemon.pid" 2>/dev/null
        done
        
        echo -e "${GREEN}✓ Tüm instance'lar durduruldu${NC}"
    else
        get_ports $id
        get_paths $id
        
        echo -e "${YELLOW}Instance $id durduruluyor...${NC}"
        
        if [[ -f "$DATA_DIR/daemon.pid" ]]; then
            local pid=$(cat "$DATA_DIR/daemon.pid")
            if kill -0 $pid 2>/dev/null; then
                kill $pid
                rm -f "$DATA_DIR/daemon.pid"
                echo -e "${GREEN}✓ Instance $id durduruldu (PID: $pid)${NC}"
            else
                echo -e "${YELLOW}Instance $id zaten çalışmıyor${NC}"
                rm -f "$DATA_DIR/daemon.pid"
            fi
        else
            # PID dosyası yoksa, port ile bul
            pkill -f "sentinel_daemon.*--tcp-port $TCP_PORT" 2>/dev/null || true
            echo -e "${GREEN}✓ Instance $id durduruldu${NC}"
        fi
    fi
}

show_status() {
    echo -e "${BLUE}SentinelFS Instance Durumları${NC}"
    echo "================================"
    
    local found=0
    
    # Ana daemon
    if pgrep -f "sentinel_daemon" > /dev/null 2>&1; then
        echo ""
        ps aux | grep "[s]entinel_daemon" | while read line; do
            found=1
            local pid=$(echo $line | awk '{print $2}')
            local cmd=$(echo $line | awk '{for(i=11;i<=NF;i++) printf "%s ", $i}')
            
            # Port bilgisini çıkar
            local tcp_port=$(echo $cmd | grep -oP '(?<=--tcp-port )\d+' || echo "8080")
            local udp_port=$(echo $cmd | grep -oP '(?<=--discovery-port )\d+' || echo "9999")
            
            echo -e "${GREEN}● Çalışıyor${NC}"
            echo "  PID:      $pid"
            echo "  TCP:      $tcp_port"
            echo "  UDP:      $udp_port"
            echo ""
        done
    fi
    
    if [[ $found -eq 0 ]]; then
        echo -e "${YELLOW}Çalışan instance bulunamadı${NC}"
    fi
}

list_ports() {
    echo -e "${BLUE}SentinelFS Port Yapılandırması${NC}"
    echo "================================"
    echo ""
    printf "%-10s %-10s %-10s %-12s %-30s\n" "Instance" "TCP" "UDP" "Metrics" "Watch Directory"
    printf "%-10s %-10s %-10s %-12s %-30s\n" "--------" "---" "---" "-------" "---------------"
    
    for i in {1..5}; do
        get_ports $i
        get_paths $i
        printf "%-10s %-10s %-10s %-12s %-30s\n" "$i" "$TCP_PORT" "$UDP_PORT" "$METRICS_PORT" "$WATCH_DIR"
    done
    
    echo ""
    echo "Ana daemon (instance 0): TCP=8080, UDP=9999, Metrics=9100"
}

show_codes() {
    echo -e "${BLUE}SentinelFS Session Codes${NC}"
    echo "========================"
    echo ""
    
    for i in {1..5}; do
        get_paths $i
        if [[ -f "$SESSION_CODE_FILE" ]]; then
            local code=$(cat "$SESSION_CODE_FILE")
            echo -e "Instance $i: ${GREEN}$code${NC}"
        else
            echo -e "Instance $i: ${YELLOW}(henüz oluşturulmadı)${NC}"
        fi
    done
    echo ""
}

regenerate_code() {
    local id=$1
    
    if [[ -z "$id" ]]; then
        echo -e "${RED}Hata: Instance ID belirtilmeli${NC}"
        echo "Kullanım: $0 gencode <instance_id>"
        exit 1
    fi
    
    if [[ ! "$id" =~ ^[1-9]$ ]]; then
        echo -e "${RED}Hata: Instance ID 1-9 arasında olmalı${NC}"
        exit 1
    fi
    
    get_paths $id
    mkdir -p "$DATA_DIR"
    
    local new_code=$(generate_session_code)
    echo "$new_code" > "$SESSION_CODE_FILE"
    chmod 600 "$SESSION_CODE_FILE"
    
    echo -e "${GREEN}Instance $id için yeni session code oluşturuldu:${NC}"
    echo -e "  ${BLUE}$new_code${NC}"
    echo ""
    echo -e "${YELLOW}Not: Bu kodu diğer cihazlarla paylaşarak güvenli bağlantı kurabilirsiniz.${NC}"
    
    # Eğer instance çalışıyorsa uyar
    if pgrep -f "sentinel_daemon.*--port $TCP_PORT" > /dev/null 2>&1; then
        echo -e "${YELLOW}⚠️  Instance $id çalışıyor. Yeni kodu kullanmak için yeniden başlatın:${NC}"
        echo "    $0 stop $id && $0 $id"
    fi
}

# Ana mantık
case "${1:-}" in
    "")
        print_usage
        ;;
    "stop")
        stop_instance "$2"
        ;;
    "status")
        show_status
        ;;
    "list")
        list_ports
        ;;
    "codes")
        show_codes
        ;;
    "gencode")
        regenerate_code "$2"
        ;;
    "help"|"-h"|"--help")
        print_usage
        ;;
    *)
        start_instance "$1"
        ;;
esac
