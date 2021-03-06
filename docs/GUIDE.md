Async HTTP for Marmalade Quick: INSTALLATION AND USAGE GUIDE
------------------------------------------------------------

Intro & Disclaimer:
-------------------
The code included in this repository is designed to provide the Marmalade Quick Framework with a simple asynchronous HTTP Downloading API, aimed primarily at downloading files in parallel as a background process.

Please note that you should have at least basic experience extending marmalade Quick or working with VS projects to integrate this library. If you don't then you should review and pay special attention to [Marmalade's documentation on this subject](http://docs.madewithmarmalade.com/display/MD/Extending+Quick). 

Also, before beginning, users should take into account that the results of this implementation may vary, especially in lower end devices in which paralell request processing will probably bog down the main (LUA) thread. Use responsibly.

*NOTE:* I wrote the guidelines for Windows systems. If you use a Mac then go ask Steve Jobs.

## Installing Async HTTP:

The easiest way to integrate this library into Marmalade Quick is to follow the official documentation for extending Quick (mentioned earlier). In it you will find a detailed guide with all the steps needed (*it's fairly straightforward when you've done it two or three times, just have patience*). The only thing you need outside of Marmalade's guide is the source code in this repo.

## Prerequisites:

- Install Marmalade (7.4 and up supported).
- Install Visual Studio [required by Marmalade C++](http://docs.madewithmarmalade.com/display/MD/Working+with+your+IDE#WorkingwithyourIDE-Marmalade'sintegrationwithVisualStudioonWindows)
- Clone quick-asyncNet repository.

## The overall steps are as follows:

### 1) COPY LIBRARY SOURCE CODE:

Copy the contents of "quickuser" in this repo to the Marmalade SDK location's "/quick/quickuser" (you will probably need to create the 'quickuser' folder).

### 2) ADD IWHTTP TO THE PROJECT PATH

Open *'MARMALADE_HOME/quick/quick.mkf'*, find the subprojects section (at the bottom of the file) and add *iwhttp*. This will include marmalade C++'s http library, required to implement async downloading in quick.

The *subprojects* section of *'quick.mkf'* should now look like this:

    subprojects
    {
        ../modules/third_party/openquick/proj.marmalade/openquick.mkf
        quickuser
        s3eFacebook
        s3eWebView
        iwbilling
        s3eFlurry
        iwhttp
    }
    
You should see GCC building iwhttp related files (such as CIwHTTP.cpp) in Marmalade's build output from now on.

### 3) ADD SOURCE FILES TO quickuser_tolua.pkg

Open *'MARMALADE_HOME/quick/quickuser_tolua.pkg'*, add the following line:

    $cfile "quickuser/AHTTP.h"

If you are starting from scratch with the default file, it sould now look like this:

    //------------------------------------------------------------------------------
    // Mark-up in header files
    //------------------------------------------------------------------------------
    $cfile "quickuser.h"
    $cfile "quickuser/AHTTP.h"

The path you enter here must be relative to your *MARMALADE_HOME/quick* directory.

### 3.1) ADD ALL SOURCES TO PROJECT

I am not exactly sure if this is required, but I do it to keep the source files in my VS project, otherwise they seem to disappear (I never said I was a pro, did I?).

Open the *'quickuser.mkf'* file in *'MARMALADE_HOME/quick'* and add references to all the files included in this library repo:

    AHTTP.h
    AHTTP.cpp
    AHTTPGlobals.h
    AHTTPLoader.h
    AHTTPLoader.cpp
    AHTTPRequest.h
    AHTTPRequest.cpp

It should look like this if you started from scratch:

    includepath .
    
    files
    {
        quickuser_tolua.cpp
        quickuser_tolua.pkg
        quickuser.h
    
	(quickuser)
	AHTTP.h
        AHTTP.cpp
        AHTTPGlobals.h
        AHTTPLoader.h
        AHTTPLoader.cpp
        AHTTPRequest.h
        AHTTPRequest.cpp
    }

*NOTE:* the *(quickuser)* notation indicates all the files below are in the *'/quickuser'* directory inside the root.

### 4) RUN LUA CODE GENERATOR:

In a new cmd console go to *'MARMALADE_HOME/quick'* and enter the following command:

    tools/tolua++.exe -o quickuser_tolua.cpp quickuser_tolua.pkg

This should have added the new LUA bindings to *'quickuser_tolua.cpp'*, If you want, you can check this by opening the cpp file and searching for a **'downloadURL'** function or the "ahttp" namespace.

### 5) BUILD MARMALADE QUICK

Find quick_prebuilt.mkb in marmalade's *'/quick'* directory, right click it and select "build". In case this fails (usually due to RVCT licensing stuff on the RVCT builds), you can allways build the targets one by one either on the right click menu or from the Visual Studio project.

Grab a cup o' Joe, it may take a while....

*NOTE*: As of Marmalade 7.4.3, I had to manually transfer the built binaries (*quick_prebuilt.s3e* and *quick_prebuilt_d.s3e* files in case of ARM) from the *targets/win* folder to their proper place in *target/arm*. This issue was addressed with the new build scripts in Marmalade [7.7.0](http://docs.madewithmarmalade.com/display/MD/Release+Notes#ReleaseNotes-7.7ReleaseNotes).

**THAT'S THAT!**

if the builds went well, then enjoy your parallel downloading!

Using ASYNC HTTP from LUA:
--------------------------
Usage is fairly simple, if properly installed and compiled, the static module **ahttp** is made available through LUA. This module exposes two methods for performing requests, one does GET and POST requests and the other is used for direct-downloading files. Both methods return a boolean value for indicating immediate errors with the call ('true' always  means success). Both methods are explained below.

**requestURL**

This method receives the remote URL, http method (*get*, *post*) and request body. The request will be performed and the response will be sent through the 'complete' *http_event*.

LUA Code sample:

    function http_handler(evt)
      if evt.status = "complete" then
        print("Request complete, response: "..evt.result)
      elseif evt.status == "error" then
        print("Request error (" .. evt.ecode .. ")")
      end
    end
    -- Add http event listener to catch complete and / or errors
    system:addEventListener("http_event", http_handler)
    -- Begin download
    ahttp.requestURL("https://api.twitter.com/1.1/help/tos.json", "get", "{}")
    
Setting request Headers:
the *ahttp* module provides get, remove and flush methods for managing the request headers. The *ahttp.requestURL* method will send the HTTP request using all headers that are set at the moment, and the header list will be cleaned. This means that headers are reset every time a request is sent.

LUA Code sample:

    -- Clear all set headers just in case
    ahttp.flushRequestHeaders()
    -- Set headers "content-type" and "custom-header" providing header names and values
    ahttp.addRequestHeader("content-type", "application/json")
    ahttp.addRequestHeader("custom-header", "custom-value")
    -- Remove header "custom-header" using the header's name
    ahttp.removeRequestHeader("custom-header")
    -- Begin download (will send the "content-type" header that was left in the list
    ahttp.requestURL("https://api.twitter.com/1.1/help/tos.json", "get", "{}")

*be aware: requests sent by the downloadURL API don't use these headers and therefore they DO NOT flush the header list.*
    
**downloadURL**

This method receives a remote URL and a local file URL as parameters. Once called, **downloadURL** will open a request to the remote URL and then proceed to download the contents into a file (thus the local URL).

Completion and error notices are sent to the LUA layer through Quick LUA's event system, by listening to the type **"http_event"** as you would normally [listen other system events](http://docs.madewithmarmalade.com/display/MD/Touch+and+Other+Events).

LUA Code sample:

    function http_handler(evt)
      if evt.status = "complete" then
        print("Download complete !")
      elseif evt.status == "error" then
        print("Download error (" .. evt.ecode .. ")")
      end
    end
    -- Add http event listener to catch complete and / or errors
    system:addEventListener("http_event", http_handler)
    -- Begin download
    ahttp.downloadURL("http://www.google.com/favicon.ico", "google_favicon.ico")
    
**IMPORTANT:** For performance reasons, the Async HTTP library has a hard limit to concurrent downloads. This limit is **5 (FIVE CONCURRENT OPERATIONS)**. Calls to **downloadURL** or **requestURL** while already processing 5 other requests will result in no action(calls to both are cumulative), and the call will return **1** instead of the normal **0** to indicate an error.

**HTTP EVENTS:**

In addition to **complete** and **error** status events, the API will dispatch **begin** (when a net connection is established) and **progress** events (for content that takes more than one cycle to read).

Here's a brief of the event parameters:

    http_event {
      status,
      ecode,
      percent,
      url,
      result,
      filename
    }

**status** values (STRING):

- "begin": Connection with remote server achieved, fetching data.
- "in_progress": Getting data from server, dispatched when a file takes time to fetch/read.
- "complete": Request completed successfully (and data dumped to file).
- "error": An error occurred (both HTTP and/or connection errors here)

**ecode** values (INT):

- 0: Operation OK, no errors, no info.
- 1: Connection Error (invalidated socket, connection timeout, bad gateway)
- 2: File Error (Unable to open file handle or failure to write data to disk)
- 3: Maximum response size exceeded (hard limit for non-download calls is 256kb)
- XXX: all values other than 0, 1 and 2 correspond to HTTP response codes (*i.e. 200 is OK, 404 is NOT FOUND*)

Other values:

- **percent** (FLOAT): a float between 0 and 1, indicating socket read progress, ingnore unless the event status is 'in_progress'.
- **url** (STRING): the request's remote URL.
- **filename** (STRING): the request's local file URL (to which the stream will be downloaded). only present for requests made through the *downloadURL()* method.
- **result** (STRING): the request's result, only present when the status code is "complete", and if the request was made through the *requestURL()* method.
