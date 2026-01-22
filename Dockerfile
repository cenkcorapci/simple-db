# Multi-stage build for SimpleDB on Alpine Linux (busybox-based)
# Stage 1: Build stage
FROM alpine:3.19 AS builder

# Set working directory
WORKDIR /build

# Install build dependencies
# Using retry logic for network issues
RUN for i in 1 2 3; do \
        apk update && \
        apk add --no-cache \
            build-base \
            cmake \
            g++ \
            make && \
        break || sleep 5; \
    done

# Copy source files
COPY CMakeLists.txt Makefile ./
COPY include ./include/
COPY src ./src/

# Build the project
RUN mkdir -p build && \
    cd build && \
    cmake .. && \
    make

# Stage 2: Runtime stage (minimal busybox-based image)
FROM alpine:3.19

# Install runtime dependencies (minimal)
# netcat-openbsd is needed for healthchecks and integration tests
RUN for i in 1 2 3; do \
        apk update && \
        apk add --no-cache \
            libstdc++ \
            libgcc \
            netcat-openbsd && \
        break || sleep 5; \
    done

# Create a non-root user
RUN addgroup -g 1000 simpledb && \
    adduser -D -u 1000 -G simpledb simpledb

# Create data directory
RUN mkdir -p /data && chown simpledb:simpledb /data

# Copy binary from builder stage
COPY --from=builder /build/build/simpledb /usr/local/bin/simpledb

# Set working directory
WORKDIR /data

# Switch to non-root user
USER simpledb

# Expose default port
EXPOSE 7777

# Default command
ENTRYPOINT ["/usr/local/bin/simpledb"]
CMD ["--port", "7777", "--log", "/data/simpledb.log"]
