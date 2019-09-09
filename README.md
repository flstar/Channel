# Channel

Golang style channel in c++ 11.

Difference from concurrent queue:
- Channel can be closed and ClosedChannelExeption will be thrown out if a sender tries to send element to closed channle, or a receiver tries to receive element from closed empty channel.
- Method try_recv(T*, timeout_us) tries to get an element from channel. It return true if an element is gotten, or false if time out. If the channel is empty and closed, CloseChannelException will be thrown out.
- Capacity limitation to control memory usage. If the channel is full, senders will be blocked.
