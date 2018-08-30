FROM debian:sid

LABEL maintainer "andrea.sessa@cleafy.com"

# Build arguments
ARG DEBIAN_REPO_HOST=httpredir.debian.org
ARG GIT_LOCATION=https://github.com/Sessa93/nginx-ssl-ja3.git
ARG GIT_BRANCH=origin/openresty

# Mirror to my location
RUN echo "deb http://${DEBIAN_REPO_HOST}/debian sid main" > /etc/apt/sources.list
RUN echo "deb-src http://${DEBIAN_REPO_HOST}/debian sid main" >> /etc/apt/sources.list

# Update
RUN DEBIAN_FRONTEND=noninteractive apt-get update || true

# Install build dependencies
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y --fix-missing \
    apt-utils \
    autoconf \
    automake \
    bind9-host \
    build-essential \
    dh-autoreconf \
    cpanminus \
    curl \
    devscripts \
    exuberant-ctags \
    git-core \
    jq \
    llvm \
    libgeoip1 \
    libgeoip-dev \
    libpcre3 \
    libpcre3-dbg \
    libpcre3-dev \
    libperl-dev \
    libmagic-dev \
    libtool \
    lsof \
    make \
    mercurial \
    ngrep \
    procps \
    python \
    telnet \
    tcpflow \
    valgrind \
    vim \
    wget \
    zlib1g \
    zlib1g-dev

# Create build directory
RUN mkdir -p /build

WORKDIR /build

# Fetches and clones from git location
RUN git clone ${GIT_LOCATION}
RUN cd nginx-ssl-ja3 && git checkout ${GIT_BRANCH}

WORKDIR /build

# Get test framework
RUN git clone https://github.com/nginx/nginx-tests

# Get openssl master from git
RUN git clone https://github.com/openssl/openssl

# Build and install openssl
WORKDIR /build/openssl
RUN ./config -d
RUN make
RUN make install

# Clone from nginx
WORKDIR /build
RUN wget https://openresty.org/download/openresty-1.13.6.2.tar.gz
RUN tar -zxvf openresty-1.13.6.2.tar.gz

# Patch nginx for fetching ssl client extensions
WORKDIR /build/openresty-1.13.6.2/bundle/nginx-1.13.6
COPY nginx.ssl.extensions.patch /build/openresty-1.13.6.2/bundle/nginx-1.13.6
RUN cat nginx.ssl.extensions.patch
RUN patch -p1 < nginx.ssl.extensions.patch

# Configure, make and install
WORKDIR /build/openresty-1.13.6.2
RUN ./configure \
    --with-luajit \
    --with-http_sub_module \
    --with-http_gunzip_module \
    --with-http_stub_status_module \
    --with-http_realip_module \
    --with-cc-opt='-O2 -fstack-protector --param=ssp-buffer-size=4 -Wformat -Werror=format-security -D_FORTIFY_SOURCE=2' \
    --with-ld-opt='-Wl,-Bsymbolic-functions -Wl,-z,relro' \
    --http-log-path=/var/log/nginx/access.log \
    --error-log-path=/var/log/nginx/error.log \
    --lock-path=/var/lock/openresty.lock \
    --pid-path=/var/run/openresty.pid \
    --http-fastcgi-temp-path=/var/lib/nginx/fastcgi \
    --http-proxy-temp-path=/var/lib/nginx/proxy \
    --http-scgi-temp-path=/var/lib/nginx/scgi \
    --http-uwsgi-temp-path=/var/lib/nginx/uwsgi \
    --add-module=/build/nginx-ssl-ja3 --with-http_ssl_module --with-stream_ssl_module --with-stream  --with-ld-opt="-L/usr/local/lib -Wl,-E"
RUN make
RUN make install

# # Install files
# RUN mkdir -p /usr/local/nginx/conf/
# COPY nginx.conf /usr/local/nginx/conf/nginx.conf

# # Install self-signed certificate
# RUN LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib /usr/local/bin/openssl req -new -x509 -days 365 -nodes -out /usr/local/nginx/conf/cert.pem -keyout /usr/local/nginx/conf/rsa.key -subj "/C=PT/ST=Lisbon/L=Lisbon/O=Development/CN=foo.local"

# # exuberant ctags
# RUN cd /build/nginx-ssl-ja3 && ctags -R src/ ../nginx/src/

# # vim config
# COPY vimrc /etc/vim/vimrc

# RUN echo 'export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib' | tee -a /root/.bashrc
# RUN echo 'export PATH=$PATH:/usr/local/bin:/usr/local/nginx/sbin' | tee -a /root/.bashrc
# RUN echo '' | tee -a /root/.bashrc
# RUN echo 'export ASAN_OPTIONS=symbolize=1' | tee -a /root/.bashrc
# RUN echo 'export export ASAN_SYMBOLIZER_PATH=/usr/bin/llvm-symbolizer' | tee -a /root/.bashrc
# RUN echo '' | tee -a /root/.bashrc

# RUN echo 'TO COMPILE RUN:\n    cd nginx\n    ASAN_OPTIONS=symbolize=1 ./auto/configure --add-module=/build/nginx-ssl-ja3 --with-http_ssl_module --with-stream_ssl_module --with-debug --with-stream --with-cc-opt="-fsanitize=address -O -fno-omit-frame-pointer" --with-ld-opt="-L/usr/local/lib -Wl,-E -lasan"\n    make install' | tee -a /build/COMPILE.ASAN.README
# RUN echo 'TO TEST RUN:\n    nginx &\n    openssl s_client -connect 127.0.0.1:12345 -cipher "AES128-SHA" -curves secp521r1' | tee -a /build/TEST.README

