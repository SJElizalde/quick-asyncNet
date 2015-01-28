/*
 * (C) 2001-2012 Marmalade. All Rights Reserved.
 *
 * This document is protected by copyright, and contains information
 * proprietary to Marmalade.
 *
 * This file consists of source code released by Marmalade under
 * the terms of the accompanying End User License Agreement (EULA).
 * Please do not use this program/source code before you have read the
 * EULA and have agreed to be bound by its terms.
 */
/*
 * This javascript code must be included into any webapp that wants to
 * make callbacks to native (C/C++) code on iOS.
 *
 * Ideally this would be inject automatically by the iOS extension but
 * I wasn't able to find a javascript injection hook that would allow
 * me to run this code on a new page before the onload handlers.  So we
 * have to ask the application to load this bit of javascript.
 */
function _iOSSetup()
{
    var frame = document.createElement('iframe');
    frame.setAttribute('style', 'display:none;');
    frame.setAttribute('height','0px');
    frame.setAttribute('width','0px');
    frame.setAttribute('frameborder','0');

    document.documentElement.appendChild(frame);

    s3e = {};
    s3e.queue = null;
    s3e.frame = frame;
    s3e.installed = 'yes';

    s3e.exec = function(jsonString)
    {
        if (s3e.queue == null)
        {
            s3e.queue = '!' + jsonString.length + '!' + jsonString;
            s3e.frame.src = 's3ebridge://queued';
        }
        else
        {
            s3e.queue += '!' + jsonString.length + '!' + jsonString;
        }
    }

    s3e.fetchqueued = function fetchqueued()
    {
        var queue = s3e.queue;
        s3e.queue = null;
        return queue;
    }
}

if (navigator.userAgent.match(/(iPad)|(iPhone)|(iPod)/i))
{
    _iOSSetup();
}
