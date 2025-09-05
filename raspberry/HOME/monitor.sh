#!/bin/bash

# Configuración: IP -> Nombre del dispositivo
declare -A IP_MAP
IP_MAP=( 
    ["192.168.1.1"]="Router"
    ["8.8.8.8"]="Google-DNS1"
    ["8.8.4.4"]="Google-DNS2"
    # Agrega tus 20 IPs con nombre
)

STATUS_FILE="/tmp/ip_status.txt"
FROM_EMAIL="tuemail@gmail.com"
TO_EMAIL="destino@gmail.com"
SUBJECT="Cambio de estado de IP"
SMTP_CONFIG="/etc/msmtprc"

# Inicializar archivo de estados si no existe
[ ! -f "$STATUS_FILE" ] && touch "$STATUS_FILE"

# Función para enviar correo
send_email() {
    local ip=$1
    local name=$2
    local state=$3
    echo "El dispositivo '$name' (IP: $ip) cambió a estado: $state" | \
    msmtp --file=/etc/msmtprc --from="$FROM_EMAIL" -t "$TO_EMAIL" -s "$SUBJECT"
}

# Comprobar cada IP
for ip in "${!IP_MAP[@]}"; do
    ping -c 1 -W 1 "$ip" > /dev/null 2>&1
    current_status=$([ $? -eq 0 ] && echo "UP" || echo "DOWN")
    previous_status=$(grep "^$ip " "$STATUS_FILE" | awk '{print $2}')

    if [ "$previous_status" != "$current_status" ]; then
        send_email "$ip" "${IP_MAP[$ip]}" "$current_status"
        # Actualizar o agregar estado
        grep -v "^$ip " "$STATUS_FILE" > "$STATUS_FILE.tmp"
        echo "$ip $current_status" >> "$STATUS_FILE.tmp"
        mv "$STATUS_FILE.tmp" "$STATUS_FILE"
    fi
done
