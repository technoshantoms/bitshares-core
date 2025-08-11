# Docker Container

This repository comes with built-in Dockerfile to support docker
containers. This README serves as documentation.

## Dockerfile Specifications

The `Dockerfile` performs the following steps:

1. Obtain base image (phusion/baseimage:0.10.1)
2. Install required dependencies using `apt-get`
3. Add acloudbank-core source code into container
4. Update git submodules
5. Perform `cmake` with build type `Release`
6. Run `make` and `make_install` (this will install binaries into `/usr/local/bin`
7. Purge source code off the container
8. Add a local acloudbank user and set `$HOME` to `/var/lib/acloudbank`
9. Make `/var/lib/acloudbank` and `/etc/acloudbank` a docker *volume*
10. Expose ports `8090` and `1776`
11. Add default config from `docker/default_config.ini` and
    `docker/default_logging.ini`
12. Add an entry point script
13. Run the entry point script by default

The entry point simplifies the use of parameters for the `witness_node`
(which is run by default when spinning up the container).

### Supported Environmental Variables

* `$ACLOUDBANKD_SEED_NODES`
* `$ACLOUDBANKD_RPC_ENDPOINT`
* `$ACLOUDBANKD_PLUGINS`
* `$ACLOUDBANKD_REPLAY`
* `$ACLOUDBANKD_RESYNC`
* `$ACLOUDBANKD_P2P_ENDPOINT`
* `$ACLOUDBANKD_WITNESS_ID`
* `$ACLOUDBANKD_PRIVATE_KEY`
* `$ACLOUDBANKD_TRACK_ACCOUNTS`
* `$ACLOUDBANKD_PARTIAL_OPERATIONS`
* `$ACLOUDBANKD_MAX_OPS_PER_ACCOUNT`
* `$ACLOUDBANKD_ES_NODE_URL`
* `$ACLOUDBANKD_TRUSTED_NODE`

### Default config

The default configuration is:

    p2p-endpoint = 0.0.0.0:1776
    rpc-endpoint = 0.0.0.0:8090
    bucket-size = [60,300,900,1800,3600,14400,86400]
    history-per-size = 1000
    max-ops-per-account = 100
    partial-operations = true

# Docker Compose

With docker compose, multiple nodes can be managed with a single
`docker-compose.yaml` file:

    version: '3'
    services:
     main:
      # Image to run
      image: acloudbank/acloudbank-core:latest
      # 
      volumes:
       - ./docker/conf/:/etc/acloudbank/
      # Optional parameters
      environment:
       - ACLOUDBANKD_ARGS=--help


    version: '3'
    services:
     fullnode:
      # Image to run
      image: acloudbank/acloudbank-core:latest
      environment:
      # Optional parameters
      environment:
       - ACLOUDBANKD_ARGS=--help
      ports:
       - "0.0.0.0:8090:8090"
      volumes:
      - "acloudbank-fullnode:/var/lib/acloudbank"


# Docker Hub

This container is properly registered with docker hub under the name:

* [acloudbank/acloudbank-core](https://hub.docker.com/r/acloudbank/acloudbank-core/)

Going forward, every release tag as well as all pushes to `develop` and
`testnet` will be built into ready-to-run containers, there.

# Docker Compose

One can use docker compose to setup a trusted full node together with a
delayed node like this:

```
version: '3'
services:

 fullnode:
  image: acloudbank/acloudbank-core:latest
  ports:
   - "0.0.0.0:8090:8090"
  volumes:
  - "acloudbank-fullnode:/var/lib/acloudbank"

 delayed_node:
  image: acloudbank/acloudbank-core:latest
  environment:
   - 'ACLOUDBANKD_PLUGINS=delayed_node witness'
   - 'ACLOUDBANKD_TRUSTED_NODE=ws://fullnode:8090'
  ports:
   - "0.0.0.0:8091:8090"
  volumes:
  - "acloudbank-delayed_node:/var/lib/acloudbank"
  links: 
  - fullnode

volumes:
 acloudbank-fullnode:
```
