# Channel

Golang style channel in c++ 11.

Difference from concurrent queue:
- Channel can be closed and ClosedChannelExeption will be thrown out if:
  - a sender tries to send an element to a closed channle, or
  - a receiver tries to get element from closed empty channel
- Methods try_send() and try_recv() to send/recv with a timeout:
  - returns true if operation succeeds, or false if times out
  - as common send/recv, ClosedChannelException may be thrown out if Channel is closed (and empty for try_recv())
- Capacity limitation to control memory usage:
  - if the channel is full, senders will be blocked
  - if the channel is empty, the receiver will be blocked
  - if capacity is 0, sender returns only after a receiver retrieves its element
