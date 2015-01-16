# quick-asyncNet
Asynchronous paralell HTTP requests for Marmalade Quick. Included APIs and LUA Bindings

Marmalade Quick offers a synchronous one-at-a-time HTTP library (uses LuaSocket) out of the box, but this is seldom enough for apps that need to download dynamic content.

This C++ Extension is an effort to provide an API to download files asynchronically and in parallel. The current implementation downlaods files directly to a specified file URL locally, and notifies LUA through a Quick Event.

For more info check out the [GUIDE](https://github.com/SJElizalde/quick-asyncNet/blob/master/docs/GUIDE.md).
