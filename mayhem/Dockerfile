# syntax=docker/dockerfile:1.7
# shadPS4 commit image: builds the file-format parser fuzz harnesses against the
# upstream sources, the same way tests/CMakeLists.txt builds its unit tests (only
# the sources under test + common/logging + tests/stubs), not the whole emulator.
FROM ghcr.io/savantenvs/base:20261023

ARG COMMIT
ARG REPO_URL
ARG SOURCE_DATE_EPOCH
ARG SANITIZER_FLAGS
ARG COVERAGE_FLAGS=
ARG MAYHEM_JOBS=

USER root
RUN [ -n "$SOURCE_DATE_EPOCH" ] && repro-sources-list.sh || true ; \
    apt-get update && apt-get install -yq --no-install-recommends libsdl3-dev \
 && rm -rf /var/lib/apt/lists/*

# The upstream submodules the harnesses need (header-only libs + fmt/spdlog), pinned
# to the exact commits upstream's externals/ gitlinks record. The CI checkout does
# not carry submodules, so they live under /opt/toolchains/deps (SPEC §6.2 item 19).
COPY mayhem/fetch-dep.sh /opt/mayhem-deps/fetch-dep.sh
RUN set -e; F="bash /opt/mayhem-deps/fetch-dep.sh"; \
    $F fmt          https://github.com/shadps4-emu/ext-fmt.git  ec73fb72477d80926c758894a3ab2cb3994fd051; \
    $F spdlog       https://github.com/gabime/spdlog.git        b8944a4bcd478ee03375c9c50dc8d6c741f43f7b; \
    $F magic_enum   https://github.com/Neargye/magic_enum.git   a413fcc9c46a020a746907136a384c227f3cd095; \
    $F robin-map    https://github.com/Tessil/robin-map.git     4ec1bf19c6a96125ea22062f38c2cf5b958e448e; \
    $F toml11       https://github.com/ToruNiina/toml11.git     a01fe3b4c14c6d7b99ee3f07c9e80058c6403097; \
    $F json         https://github.com/nlohmann/json.git        55f93686c01528224f448c19128836e7df245f72; \
    D=/opt/toolchains/deps; \
    cmake -S $D/fmt-ec73fb7/src -B /tmp/b-fmt -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DCMAKE_CXX_FLAGS="-g -gdwarf-3" -DFMT_TEST=OFF -DFMT_DOC=OFF -DFMT_INSTALL=ON \
      -DCMAKE_INSTALL_PREFIX=$D/fmt-ec73fb7 -DCMAKE_INSTALL_LIBDIR=lib \
 && cmake --build /tmp/b-fmt && cmake --install /tmp/b-fmt \
 && cmake -S $D/spdlog-b8944a4/src -B /tmp/b-spd -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DCMAKE_CXX_FLAGS="-g -gdwarf-3" -DSPDLOG_FMT_EXTERNAL=ON -DSPDLOG_NO_EXCEPTIONS=ON \
      -DSPDLOG_DISABLE_GLOBAL_LOGGER=ON -DSPDLOG_BUILD_EXAMPLE=OFF -DSPDLOG_INSTALL=ON \
      -DCMAKE_PREFIX_PATH=$D/fmt-ec73fb7 -DCMAKE_INSTALL_PREFIX=$D/spdlog-b8944a4 -DCMAKE_INSTALL_LIBDIR=lib \
 && cmake --build /tmp/b-spd && cmake --install /tmp/b-spd \
 && rm -rf /tmp/b-fmt /tmp/b-spd && chmod -R a+rX $D

USER mayhem
WORKDIR /mayhem
COPY --chown=2000:2000 . /mayhem

RUN if [ -x /opt/mayhem/ccache/prefill ]; then /opt/mayhem/ccache/prefill bash mayhem/build.sh; \
    else bash mayhem/build.sh; fi

RUN if [ -f mayhem/test.sh ]; then bash mayhem/test.sh; else echo "no mayhem/test.sh"; fi

LABEL org.opencontainers.image.source="${REPO_URL}" \
      org.opencontainers.image.revision="${COMMIT}" \
      rlenv.tier="commit" \
      rlenv.source_date_epoch="${SOURCE_DATE_EPOCH}" \
      rlenv.user="mayhem"

USER mayhem
WORKDIR /mayhem
