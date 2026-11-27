# Lexify (Logix compiler + website) for Render / any Docker host
FROM node:22-bookworm-slim

# C compiler + flex + bison to build the Logix compiler on Linux
RUN apt-get update \
 && apt-get install -y --no-install-recommends build-essential flex bison \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

# Build the compiler fresh (never use a Windows logix.exe)
RUN rm -rf build logix logix.exe && make && make test

# Website + API server
WORKDIR /app/experiments/backend
RUN npm ci --omit=dev

ENV NODE_ENV=production \
    LOGIX_BIN=/app/logix \
    FRONTEND_DIR=/app/experiments/frontend \
    DB_PATH=/app/experiments/backend/data/logix.db \
    SESSION_TTL_HOURS=24 \
    PORT=3000
EXPOSE 3000
CMD ["node", "src/server.js"]
