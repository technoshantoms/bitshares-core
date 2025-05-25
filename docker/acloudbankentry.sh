#!/bin/bash
ACLOUDBANKD="/usr/local/bin/witness_node"

# For blockchain download
VERSION=`cat /etc/acloudbank/version`

## Supported Environmental Variables
#
#   * $ACLOUDBANKD_SEED_NODES
#   * $ACLOUDBANKD_RPC_ENDPOINT
#   * $ACLOUDBANKD_PLUGINS
#   * $ACLOUDBANKD_REPLAY
#   * $ACLOUDBANKD_RESYNC
#   * $ACLOUDBANKD_P2P_ENDPOINT
#   * $ACLOUDBANKD_WITNESS_ID
#   * $ACLOUDBANKD_PRIVATE_KEY
#   * $ACLOUDBANKD_TRACK_ACCOUNTS
#   * $ACLOUDBANKD_PARTIAL_OPERATIONS
#   * $ACLOUDBANKD_MAX_OPS_PER_ACCOUNT
#   * $ACLOUDBANKD_ES_NODE_URL
#   * $ACLOUDBANKD_ES_START_AFTER_BLOCK
#   * $ACLOUDBANKD_TRUSTED_NODE
#

ARGS=""
# Translate environmental variables
if [[ ! -z "$ACLOUDBANKD_SEED_NODES" ]]; then
    for NODE in $ACLOUDBANKD_SEED_NODES ; do
        ARGS+=" --seed-node=$NODE"
    done
fi
if [[ ! -z "$ACLOUDBANKD_RPC_ENDPOINT" ]]; then
    ARGS+=" --rpc-endpoint=${ACLOUDBANKD_RPC_ENDPOINT}"
fi

if [[ ! -z "$ACLOUDBANKD_REPLAY" ]]; then
    ARGS+=" --replay-blockchain"
fi

if [[ ! -z "$ACLOUDBANKD_RESYNC" ]]; then
    ARGS+=" --resync-blockchain"
fi

if [[ ! -z "$ACLOUDBANKD_P2P_ENDPOINT" ]]; then
    ARGS+=" --p2p-endpoint=${ACLOUDBANKD_P2P_ENDPOINT}"
fi

if [[ ! -z "$ACLOUDBANKD_WITNESS_ID" ]]; then
    ARGS+=" --witness-id=$ACLOUDBANKD_WITNESS_ID"
fi

if [[ ! -z "$ACLOUDBANKD_PRIVATE_KEY" ]]; then
    ARGS+=" --private-key=$ACLOUDBANKD_PRIVATE_KEY"
fi

if [[ ! -z "$ACLOUDBANKD_TRACK_ACCOUNTS" ]]; then
    for ACCOUNT in $ACLOUDBANKD_TRACK_ACCOUNTS ; do
        ARGS+=" --track-account=$ACCOUNT"
    done
fi

if [[ ! -z "$ACLOUDBANKD_PARTIAL_OPERATIONS" ]]; then
    ARGS+=" --partial-operations=${ACLOUDBANKD_PARTIAL_OPERATIONS}"
fi

if [[ ! -z "$ACLOUDBANKD_MAX_OPS_PER_ACCOUNT" ]]; then
    ARGS+=" --max-ops-per-account=${ACLOUDBANKD_MAX_OPS_PER_ACCOUNT}"
fi

if [[ ! -z "$ACLOUDBANKD_ES_NODE_URL" ]]; then
    ARGS+=" --elasticsearch-node-url=${ACLOUDBANKD_ES_NODE_URL}"
fi

if [[ ! -z "$ACLOUDBANKD_ES_START_AFTER_BLOCK" ]]; then
    ARGS+=" --elasticsearch-start-es-after-block=${ACLOUDBANKD_ES_START_AFTER_BLOCK}"
fi

if [[ ! -z "$ACLOUDBANKD_TRUSTED_NODE" ]]; then
    ARGS+=" --trusted-node=${ACLOUDBANKD_TRUSTED_NODE}"
fi

## Link the acloudbank config file into home
## This link has been created in Dockerfile, already
ln -f -s /etc/acloudbank/config.ini /var/lib/acloudbank
ln -f -s /etc/acloudbank/logging.ini /var/lib/acloudbank
ln -f -s /usr/local/bin/entrypoint.sh /var/lib/acloudbank

chown -R acloudbank:acloudbank /var/lib/acloudbank

# Get the latest security updates
apt-get update && apt-get upgrade -y -o Dpkg::Options::="--force-confold"

# Plugins need to be provided in a space-separated list, which
# makes it necessary to write it like this
if [[ ! -z "$ACLOUDBANKD_PLUGINS" ]]; then
   exec /usr/bin/setpriv --reuid=acloudbank --regid=acloudbank --clear-groups \
     "$ACLOUDBANKD" --data-dir "${HOME}" ${ARGS} ${ACLOUDBANKD_ARGS} --plugins "${ACLOUDBANKD_PLUGINS}"
else
   exec /usr/bin/setpriv --reuid=acloudbank --regid=acloudbank --clear-groups \
     "$ACLOUDBANKD" --data-dir "${HOME}" ${ARGS} ${ACLOUDBANKD_ARGS}
fi
