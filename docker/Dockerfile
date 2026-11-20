# Build environment for OpenOrbis PS4 homebrew (Linux amd64: clang/lld 12 + .NET deps for PkgTool)
FROM ubuntu:20.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
      make clang-12 lld-12 libicu66 libssl1.1 ca-certificates && \
    ln -s /usr/bin/clang-12 /usr/bin/clang && ln -s /usr/bin/clang++-12 /usr/bin/clang++ && \
    ln -s /usr/bin/ld.lld-12 /usr/bin/ld.lld && rm -rf /var/lib/apt/lists/*
