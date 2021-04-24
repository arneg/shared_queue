This repository contains the implementation of a bounded, lock-free
single-producer single-consumer shared memory queue. It uses the well-known
concept of a shared circular buffer and atomic instructions to safely
synchronize between producer and consumer.

When the queue is full (or empty) the writer (or reader) can optionally
wait to be woken up using a futex. This implementation supports linux only.

Copyright (C) 2020-2021 Arne Goedeke - All rights reserved.
You may use, distribute and modify this code under the terms of the BSD
license.
