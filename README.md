# Channel

Golang style channel in c++ 11.

Differences from concurrent queue:
- Channel can be closed and ClosedChannelExeption will be thrown out when receiver tries to get an element from closed empty channel.  
- Method try_recv tries to get an element from channel in timeout_us microseconds. It returns true if an valid element is received, or return false if time out.
- Capacity limitation to control memory usage. If the channel is full, senders will be blocked. If capacity is 0, sender and receiver and synchronized.
