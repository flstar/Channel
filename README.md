# Channel

Golang style channel in c++ 11.

Difference from concurrent queue:
- Channel can be closed and ClosedChannelExeption will be thrown out when receiver tries to get an element from closed empty channel.  
- Method try_recv_timeout(timeout_us) tries to get an element from channel. TimeoutException will be thrown out if it cannot get any element from channel for timeout_us micro-seconds.
- Capacity limitation to control memory usage. If the channel is full, senders will be blocked.
