#!/bin/bash

# Configuración: IP -> Nombre del dispositivo
declare -A IP_MAP
IP_MAP=( 
    ["192.168.1.1"]="Router"
    ["8.8.8.8"]="Google-DNS1"
    ["8.8.4.4"]="Google-DNS2"
    # Agrega tus 20 IPs con nombre
)

# Archivos
STATUS_FILE="/var/lib/aptelliot/ip_status.txt"
LOG_FILE="/var/log/monitor.log"
FROM_EMAIL="tuemail@gmail.com"
TO_EMAIL="destino@gmail.com"
SUBJECT="Cambio de estado de IP"
SMTP_CONFIG="/etc/msmtprc"

# Inicializar archivos si no existen
mkdir -p /var/lib/aptelliot
touch "$STATUS_FILE"
touch "$LOG_FILE"

# Función para enviar correo
send_email() {
    local ip=$1
    local name=$2
    local state=$3
    {
        echo -e "Subject: $SUBJECT\n\nEl dispositivo '$name' (IP: $ip) cambió a estado: $state"
    } | msmtp --file="$SMTP_CONFIG" --from="$FROM_EMAIL" "$TO_EMAIL"
    echo "$(date '+%Y-%m-%d %H:%M:%S') - Enviado correo: $name ($ip) -> $state" >> "$LOG_FILE"
}

# Comprobar cada IP
for ip in "${!IP_MAP[@]}"; do
    ping -c 1 -W 1 "$ip" > /dev/null 2>&1
    current_status=$([ $? -eq 0 ] && echo "UP" || echo "DOWN")
    previous_status=$(grep "^$ip " "$STATUS_FILE" | awk '{print $2}')

    # Log del ping
    echo "$(date '+%Y-%m-%d %H:%M:%S') - Ping $ip (${IP_MAP[$ip]}): $current_status" >> "$LOG_FILE"

    # Si cambió el estado, enviar correo y log
    if [ "$previous_status" != "$current_status" ]; then
        send_email "$ip" "${IP_MAP[$ip]}" "$current_status"
        # Actualizar o agregar estado
        grep -v "^$ip " "$STATUS_FILE" > "$STATUS_FILE.tmp"
        echo "$ip $current_status" >> "$STATUS_FILE.tmp"
        mv "$STATUS_FILE.tmp" "$STATUS_FILE"
        echo "$(date '+%Y-%m-%d %H:%M:%S') - Estado actualizado: $ip -> $current_status" >> "$LOG_FILE"
    fi
done
