---
layout: doc
title: Python Bindings
category: doc
---

For easy interaction with the ichabod server, there are convenient
[python bindings
available](http://github.com/monetate/ichabod-python). Assuming
ichabod is already running on port 9191, rasterization requests can be
made directly:

    from ichabod import IchabodClient
    client = IchabodClient(port=9191, timeout=1)
    result = client.rasterize(html='<h1>Hello, world!</h1>', width=105)

Or for quick one-off requests:

    from ichabod import rasterize
    result = rasterize(html='<h1>Hello, world!</h1>', port=9191, width=105)

See the [ichabod-python github
repo](http://github.com/monetate/ichabod-python) for details. Also,
check out the [iPython notebook walkthrough]({% post_url documentation/2015-06-11-ipynb-walkthrough %}).
