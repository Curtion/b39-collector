# 构建阶段 1: 构建前端
FROM node:22-alpine AS web-builder

WORKDIR /app/web

# 安装 pnpm
RUN npm install -g pnpm

# 复制前端依赖文件
COPY web/package.json web/pnpm-lock.yaml* ./

# 安装依赖
RUN pnpm install

# 复制前端源代码
COPY web/ ./

# 构建前端
RUN pnpm run build

# 构建阶段 2: 构建 Go 后端
FROM golang:1.25-alpine AS go-builder

WORKDIR /app

# 安装构建依赖
RUN apk add --no-cache gcc musl-dev sqlite-dev

# 复制 go mod 文件
COPY go.mod go.sum ./

# 下载依赖
RUN go mod download

# 复制后端源代码
COPY main.go ./

# 从前端构建阶段复制 dist 目录
COPY --from=web-builder /app/web/dist ./web/dist

# 构建 Go 应用
RUN CGO_ENABLED=1 GOOS=linux go build -a -installsuffix cgo -o b39-server .

# 最终镜像: 最小化运行环境
FROM alpine:latest

WORKDIR /app

# 安装 SQLite 运行时依赖
RUN apk add --no-cache ca-certificates sqlite-libs

# 从构建阶段复制可执行文件
COPY --from=go-builder /app/b39-server ./

# 创建数据目录
RUN mkdir -p /data

# 暴露端口
EXPOSE 8080

# 设置数据卷
VOLUME ["/data"]

# 设置环境变量指定数据库路径
ENV DB_PATH=/data/sensor.db

# 启动应用
ENTRYPOINT ["./b39-server"]
