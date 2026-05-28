# HTTPS setup

This project is an Express website/API served by `server.js` on port `3001` by default.

There are two supported HTTPS modes:

1. Local development HTTPS directly from Node using a self-signed certificate.
2. Production HTTPS through Nginx with Let’s Encrypt certificates, proxying to the Node app over local HTTP.

## Local development HTTPS

Generate a local self-signed certificate:

```bash
npm run cert:local
```

Start the server over HTTPS:

```bash
npm run dev:https
```

Then open:

```text
https://localhost:3001
```

Your browser will warn that the certificate is self-signed. For local testing, you can accept the warning or import/trust `certs/localhost.pem` in your operating system/browser trust store.

### Custom certificate paths

By default, local HTTPS expects:

```text
certs/localhost-key.pem
certs/localhost.pem
```

You can override these paths:

```bash
HTTPS=true SSL_KEY=/path/to/key.pem SSL_CERT=/path/to/cert.pem npm start
```

You can also override the port:

```bash
PORT=3443 npm run dev:https
```

## Normal local HTTP

For normal HTTP development:

```bash
npm start
```

Then open:

```text
http://localhost:3001
```

## Production HTTPS with Nginx and Let’s Encrypt

Recommended production architecture:

```text
Internet -> Nginx :443 HTTPS -> Node/Express :3001 HTTP on localhost
```

This keeps certificate renewal and TLS hardening in Nginx, while the Express app remains simple. `server.js` already enables `app.set('trust proxy', true)`, so Express can read forwarded client IP/protocol headers from Nginx.

### 1. Run the Node app on the server

Install dependencies and start the app:

```bash
npm install
PORT=3001 npm start
```

In production, run it under a process manager such as `systemd` or `pm2` so it restarts automatically.

### 2. Install Nginx and Certbot

On Ubuntu/Debian:

```bash
sudo apt update
sudo apt install nginx certbot python3-certbot-nginx
```

### 3. Configure Nginx

Copy the example config:

```bash
sudo cp nginx/ebook.conf /etc/nginx/sites-available/ebook.conf
sudo ln -s /etc/nginx/sites-available/ebook.conf /etc/nginx/sites-enabled/ebook.conf
```

Edit the domain in the file:

```bash
sudo nano /etc/nginx/sites-available/ebook.conf
```

Replace every `ebook.example.com` with your real domain.

Test and reload Nginx:

```bash
sudo nginx -t
sudo systemctl reload nginx
```

### 4. Issue a Let’s Encrypt certificate

After your DNS points to the server:

```bash
sudo certbot --nginx -d your-domain.example.com
```

Certbot will install/update the certificate paths and configure renewal.

Verify automatic renewal:

```bash
sudo certbot renew --dry-run
```

## Notes

- Do not commit generated certificates or private keys. The `certs/` directory keeps only `.gitkeep` in Git.
- For production, keep Node listening on `127.0.0.1:3001` behind Nginx instead of exposing port `3001` publicly.
- If you use another reverse proxy such as Caddy or Traefik, keep forwarding `X-Forwarded-For` and `X-Forwarded-Proto` headers.
