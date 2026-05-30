# Pierre

[![Test](https://github.com/tueda/pierre/actions/workflows/test.yml/badge.svg)](https://github.com/tueda/pierre/actions?query=branch%3Amain)

Built on [FLINT](https://flintlib.org/).

## Installation

```bash
# Install required packages (Debian/Ubuntu).
sudo apt-get update
sudo DEBIAN_FRONTEND=noninteractive apt-get install --no-install-recommends -y \
    autoconf-archive \
    automake \
    ca-certificates \
    cmake \
    curl \
    g++ \
    git \
    libtool \
    make \
    pkg-config \
    unzip \
    zip

# Clone the repository.
git clone https://github.com/tueda/pierre.git
cd pierre

# Check out the latest version tag.
git checkout "$(git tag --sort=-v:refname | head -n 1)"

# Set up vcpkg to manage dependencies.
git clone https://github.com/microsoft/vcpkg.git
./vcpkg/bootstrap-vcpkg.sh

# Configure and build the project.
cmake --preset release
cmake --build build/release --parallel

# Install the built artifacts.
cmake --install build/release --prefix /path/to/install
```

## Development

```bash
# Install required packages (Debian/Ubuntu).
sudo apt-get update
sudo DEBIAN_FRONTEND=noninteractive apt-get install --no-install-recommends -y \
    clang-tidy \
    gcovr \
    pre-commit

# Install pre-commit hooks.
pre-commit install

# Run all pre-commit checks.
pre-commit run --all-files

# Debug build.
cmake --preset debug
cmake --build build/debug --parallel
cmake --build build/debug --target check --parallel

# Clang-tidy.
cmake --preset clang-tidy
cmake --build build/clang-tidy --parallel

# Code coverage.
cmake --preset coverage
cmake --build build/coverage --target coverage --parallel
```
