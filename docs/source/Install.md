# Deployment

<div align=center><img src="https://github.com/laoshanxi/app-mesh/raw/main/docs/source/deploy.png"/></div>

App Mesh can deploy with multiple ways, generally, App Mesh run on a host as a daemon service managed by native systemd or docker container.

### Quick install by docker container
Start App Mesh daemon docker container with 4g memory limited:
```
docker run -d --memory=8g --restart=always --name=appmesh --net=host -v /var/run/docker.sock:/var/run/docker.sock laoshanxi/appmesh
```
The startup support use environment variable override default configuration with format `APPMESH_${BASE-JSON-KEY}_${SUB-JSON-KEY}=NEW_VALUE`, E.g. `export APPMESH_Security_JWTEnabled=false`, `export APPMESH_REST_HttpThreadPoolSize=10`.

### Native installation
Install App Mesh as standalone mode on local node without GUI service by release packages.

```text
# centos
sudo yum install appmesh-1.9.0-1.x86_64.rpm
# ubuntu
sudo apt install appmesh_1.9.0_amd64.deb
# SUSE
sudo zypper install appmesh-1.9.0-1.x86_64.rpm
```

Start service:
```
$ systemctl enable appmesh
$ systemctl start appmesh
$ systemctl status appmesh
● appmesh.service - App Mesh daemon service
   Loaded: loaded (/etc/systemd/system/appmesh.service; enabled; vendor preset: disabled)
```

Note:
1. On windows WSL ubuntu, use `service appmesh start` to force service start, WSL VM does not have full init.d and systemd
2. Use env `export APPMESH_FRESH_INSTALL=Y` to enable fresh installation (otherwise, SSL and configuration file will reuse previous files on this host)
3. The installation will create `appmesh` Linux user for default app running
4. On SUSE, use `sudo zypper install net-tools-deprecated` to install ifconfig tool before install App Mesh
5. The installation media structure is like this:
```
    $ tree -L 1 /opt/appmesh/
    /opt/appmesh/
    ├── appc                              -------- command line binary
    ├── apprest -> /opt/appmesh/appsvc    -------- rest service soft link
    ├── appsvc                            -------- service binary
    ├── appsvc.json                       -------- configuration file (can be modified manually or update from GUI)
    ├── lib64
    ├── log                               -------- service log dir
    ├── script
    ├── sdk                               -------- SDK binary dir
    ├── ssl                               -------- SSL certification files
    └── work                              -------- child app work dir (app log files will write in this dir)
```

### Docker compose installation with GUI and Consul Service
A simple way deploy appmesh, appmesh-ui and consul by docker-compose.

Install docker-compose:
```
sudo curl -L "https://github.com/docker/compose/releases/download/1.27.4/docker-compose-$(uname -s)-$(uname -m)" -o /usr/local/bin/docker-compose
sudo chmod +x /usr/local/bin/docker-compose
```

Get integrated docker compose file [docker-compose.yaml](https://github.com/laoshanxi/app-mesh/raw/main/script/docker-compose.yaml) and configure correct Consul bind IP address and network device name.
```
$ mkdir appmesh
$ cd appmesh
$ wget -O docker-compose.yaml https://github.com/laoshanxi/app-mesh/raw/main/script/docker-compose.yaml

$ docker-compose -f docker-compose.yaml up -d
Creating apppmesh_appmesh_1    ... done
Creating apppmesh_consul_1     ... done
Creating apppmesh_appmesh-ui_1 ... done

$ docker-compose -f docker-compose.yaml ps
        Name                       Command               State   Ports
----------------------------------------------------------------------
apppmesh_appmesh-ui_1   nginx -g daemon off;             Up
apppmesh_appmesh_1      /opt/appmesh/script/appmes ...   Up
apppmesh_consul_1       docker-entrypoint.sh consu ...   Up
```

By default, App Mesh will connect to local Consul URL with "https://127.0.0.1:443", this address is configured with `Nginx` reverse proxy route to "http://127.0.0.1:8500".

App Mesh UI is listen at `443` port with SSL protocol, open `https://appmesh_node` to access with `admin` user and Admin123 for initial password.

For production environment, Consul is better to be a cluster with 3+ server agent, one Consul agent is used for test scenario.

### Join a App Mesh node to a Consul cluster

#### Option 1: Update configuration
When installed a new App Mesh node and want to connect to existing cluster, just need configure Consul URL parameter in `/opt/appmesh/appsvc.json`:
```
  "Consul": {
    "url": "https://192.168.3.1",
  }
```
If App Mesh is running in Docker container, need mount `/opt/appmesh/appsvc.json` out of container to persist the configuration. After configuration change, just restart App Mesh container. 

#### Option 2: Update from UI
All configuration update from UI support hot-update, no need restart App Mesh process to take effect. Click `Configuration` -> `Consul` and set `Consul URL`, Click `Submit` to take effect.


---
### Usage scenarios
1. Integrate with rpm installation script and register rpm startup behavior to appmesh
2. Remote sync/async shell execute (web ssh)
3. Host/app resource monitor
4. Run as a standalone JWT server
5. File server
6. Micro service management
7. Cluster application deployment
