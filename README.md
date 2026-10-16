# JAV-Qt
## Supported Providers
JavLib
## Prerequisites
### Docker
(Gentoo) Install with `sudo emerge app-containers/docker app-containers/docker-cli` <br>
(Windows) Install with `winget install -e --id Docker.DockerDesktop`
### FlareSolverr
Install with `docker run -d --name=flaresolverr -p 127.0.0.1:8191:8191 -e LOG_LEVEL=info --restart unless-stopped ghcr.io/flaresolverr/flaresolverr:latest`
### Installation
Once you installed the prerequisites and started FlareSolverr; extract and run.