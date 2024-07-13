# The image for building
FROM phusion/baseimage:focal-1.2.0 as build
ENV LANG=en_US.UTF-8

# Install dependencies
RUN \
    apt-get update && \
    apt-get upgrade -y -o Dpkg::Options::="--force-confold" && \
    apt-get update && \
    apt-get install -y \
      g++ \
      autoconf \
      cmake \
      git \
      libbz2-dev \
      libcurl4-openssl-dev \
      libssl-dev \
      libncurses-dev \
      libboost-thread-dev \
      libboost-iostreams-dev \
      libboost-date-time-dev \
      libboost-system-dev \
      libboost-filesystem-dev \
      libboost-program-options-dev \
      libboost-chrono-dev \
      libboost-test-dev \
      libboost-context-dev \
      libboost-regex-dev \
      libboost-coroutine-dev \
      libtool \
      doxygen \
      ca-certificates \
    && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

ADD . /acloudbank-core
WORKDIR /acloudbank-core

# Compile
RUN \
    ( git submodule sync --recursive || \
      find `pwd`  -type f -name .git | \
	while read f; do \
	  rel="$(echo "${f#$PWD/}" | sed 's=[^/]*/=../=g')"; \
	  sed -i "s=: .*/.git/=: $rel/=" "$f"; \
	done && \
      git submodule sync --recursive ) && \
    git submodule update --init --recursive && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
	-DGRAPHENE_DISABLE_UNITY_BUILD=ON \
        . && \
    make witness_node cli_wallet get_dev_key && \
    install -s programs/witness_node/witness_node \
               programs/genesis_util/get_dev_key \
               programs/cli_wallet/cli_wallet \
            /usr/local/bin && \
    #
    # Obtain version
    mkdir -p /etc/acloudbank && \
    git rev-parse --short HEAD > /etc/acloudbank/version && \
    cd / && \
    rm -rf /acloudbank-core

# The final image
FROM phusion/baseimage:focal-1.2.0
LABEL maintainer="The acloudbank decentralized organisation"
ENV LANG=en_US.UTF-8

# Install required libraries
RUN \
    apt-get update && \
    apt-get upgrade -y -o Dpkg::Options::="--force-confold" && \
    apt-get update && \
    apt-get install --no-install-recommends -y \
      libcurl4 \
      ca-certificates \
    && \
    mkdir -p /etc/acloudbank && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

COPY --from=build /usr/local/bin/* /usr/local/bin/
COPY --from=build /etc/acloudbank/version /etc/acloudbank/

WORKDIR /
RUN groupadd -g 10001 acloudbank
RUN useradd -u 10000 -g acloudbank -s /bin/bash -m -d /var/lib/acloudbank --no-log-init acloudbank
ENV HOME /var/lib/acloudbank
RUN chown acloudbank:acloudbank -R /var/lib/acloudbank

# default exec/config files
ADD docker/default_config.ini /etc/acloudbank/config.ini
ADD docker/default_logging.ini /etc/acloudbank/logging.ini
ADD docker/acloudbankentry.sh /usr/local/bin/acloudbankentry.sh
RUN chmod a+x /usr/local/bin/acloudbankentry.sh

# Volume
VOLUME ["/var/lib/acloudbank", "/etc/acloudbank"]

# rpc service:
EXPOSE 8090
# p2p service:
EXPOSE 1776

# Make Docker send SIGINT instead of SIGTERM to the daemon
STOPSIGNAL SIGINT

# Temporarily commented out due to permission issues caused by older versions, to be restored in a future version
#USER acloudbank:acloudbank

# default execute entry
ENTRYPOINT ["/usr/local/bin/acloudbankentry.sh"]
