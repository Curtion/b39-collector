# Docker 镜像构建脚本
# PowerShell

$IMAGE_NAME = "ccr.ccs.tencentyun.com/curtion/b39-collector"
$VERSION = "latest"

# 切换到脚本所在目录
Push-Location (Split-Path -Parent $MyInvocation.MyCommand.Path)

Write-Host "开始构建 Docker 镜像..." -ForegroundColor Green
Write-Host "镜像: ${IMAGE_NAME}:${VERSION}" -ForegroundColor Yellow

# 构建镜像
docker build -t "${IMAGE_NAME}:${VERSION}" .

if ($LASTEXITCODE -ne 0) {
    Write-Host "镜像构建失败!" -ForegroundColor Red
    Pop-Location
    exit 1
}

Write-Host "镜像构建成功!" -ForegroundColor Green

# 推送到镜像仓库
Write-Host "推送到镜像仓库..." -ForegroundColor Green
docker push "${IMAGE_NAME}:${VERSION}"

if ($LASTEXITCODE -ne 0) {
    Write-Host "镜像推送失败!" -ForegroundColor Red
    Pop-Location
    exit 1
}

Write-Host "镜像推送成功!" -ForegroundColor Green
Write-Host "镜像地址: ${IMAGE_NAME}:${VERSION}" -ForegroundColor Yellow

Pop-Location
