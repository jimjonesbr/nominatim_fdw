#!/bin/bash

NETWORK_NAME=pgnet
PROXY_PORT=3128
PROXY_AUTH_PORT=3129

# Deploy Squid proxy WITHOUT authentication
echo -e "\n== Deploying Squid proxy (no auth) ==\n"

podman stop squid-no-auth 2>/dev/null || true
podman rm squid-no-auth 2>/dev/null || true

cat > /tmp/squid-no-auth.conf <<EOF
http_port 3128
acl localnet src 10.0.0.0/8
acl localnet src 172.16.0.0/12
http_access allow localnet
http_access deny all
EOF

podman run -d --name squid-no-auth \
  --network $NETWORK_NAME \
  --ip 172.19.42.100 \
  --no-hosts \
  -v /tmp/squid-no-auth.conf:/etc/squid/squid.conf:ro,z \
  ubuntu/squid:latest

echo "Waiting for Squid (no auth) to start..."
sleep 1

# Verify Squid is running
if podman exec squid-no-auth squid -k check 2>/dev/null; then
    echo "Squid (no auth) is ready!"
else
    echo "ERROR: Squid failed to start"
    podman logs squid-no-auth
    exit 1
fi

# Deploy Squid proxy WITH authentication
echo -e "\n== Deploying Squid proxy (with auth) ==\n"

podman stop squid-auth 2>/dev/null || true
podman rm squid-auth 2>/dev/null || true

# Create password file (user: proxyuser, password: proxypass)
# Using openssl to create bcrypt hash instead of htpasswd
echo -n "proxyuser:" > /tmp/squid-passwords
openssl passwd -apr1 proxypass >> /tmp/squid-passwords

cat > /tmp/squid-auth.conf <<EOF
http_port 3128
auth_param basic program /usr/lib/squid/basic_ncsa_auth /etc/squid/passwords
auth_param basic children 5
auth_param basic realm Squid proxy
auth_param basic credentialsttl 2 hours
acl authenticated proxy_auth REQUIRED
http_access allow authenticated
http_access deny all
EOF

podman run -d --name squid-auth \
  --network $NETWORK_NAME \
  --ip 172.19.42.101 \
  --no-hosts \
  -v /tmp/squid-auth.conf:/etc/squid/squid.conf:ro,z \
  -v /tmp/squid-passwords:/etc/squid/passwords:ro,z \
  ubuntu/squid:latest

echo "Waiting for Squid (with auth) to start..."
sleep 1

# Verify Squid is running
if podman exec squid-auth squid -k check 2>/dev/null; then
    echo "Squid (with auth) is ready!"
else
    echo "ERROR: Squid (with auth) failed to start"
    podman logs squid-auth
    exit 1
fi

BASICAUTH_PORT=3130
NOMINATIM_HOST=nominatim.openstreetmap.org

echo -e "\n== Deploying Squid reverse-proxy with HTTP Basic Auth (fronting real Nominatim) ==\n"

podman stop squid-basicauth 2>/dev/null || true
podman rm squid-basicauth 2>/dev/null || true

echo -n "nominatimuser:" > /tmp/squid-basicauth-passwords
openssl passwd -apr1 nominatimpass >> /tmp/squid-basicauth-passwords

cat > /tmp/squid-basicauth.conf <<EOF
http_port 3128 accel defaultsite=$NOMINATIM_HOST
cache_peer $NOMINATIM_HOST parent 443 0 no-query originserver ssl sslflags=DONT_VERIFY_PEER sni=nominatim.openstreetmap.org name=nominatim_backend
cache_peer_access nominatim_backend allow all

auth_param basic program /usr/lib/squid/basic_ncsa_auth /etc/squid/passwords
auth_param basic children 5
auth_param basic realm Nominatim FDW test
auth_param basic credentialsttl 2 hours

acl authenticated proxy_auth REQUIRED
http_access allow authenticated
http_access deny all
EOF

podman run -d --name squid-basicauth \
  --network $NETWORK_NAME \
  --ip 172.19.42.102 \
  --no-hosts \
  -p $BASICAUTH_PORT:3128 \
  -v /tmp/squid-basicauth.conf:/etc/squid/squid.conf:ro,z \
  -v /tmp/squid-basicauth-passwords:/etc/squid/passwords:ro,z \
  ubuntu/squid:latest

echo "Waiting for Squid (basic auth) to start..."
sleep 1

if podman exec squid-basicauth squid -k check 2>/dev/null; then
    echo "Squid (basic auth) is ready!"
else
    echo "ERROR: Squid (basic auth) failed to start"
    podman logs squid-basicauth
    exit 1
fi

# Sanity checks from the host before handing off to the SQL regress test
echo -e "\n-- sanity check: expect 401 without credentials --"
curl -s -o /dev/null -w "%{http_code}\n" http://localhost:$BASICAUTH_PORT/search?q=test

echo -e "\n-- sanity check: expect 200 with credentials --"
curl -s -o /dev/null -w "%{http_code}\n" -u nominatimuser:nominatimpass http://localhost:$BASICAUTH_PORT/search?q=test