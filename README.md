# [Ichabod](http://ichabod.org) - Fast HTML rasterizer

Ichabod is a server process which accepts POSTed HTML and can
rasterize (eg. render images) and evaluate JS in that HTML
document. There is a focus on rendering speed.

It utilizes webkit from Qt to render HTML and evaluate JavaScript, and
[mongoose](https://github.com/cesanta/mongoose) to parse incoming
requests and return responses.


## Features

- **Fast rasterization** of HTML.
- **JSON API** 
- **Javascript interface** for rasterization functions
- **Animated output** using giflib. 
- **High quality animated output** using mediancut and other diffusion methods.
- **Python bindings** available
- **X11 not required**.
- **statsd support** built in.

## Related projects

Several excellent headless projects have previously blazed a trail:

- [PhantomJS](http://phantomjs.org) scriptable headless webkit.
- [Wkhtmltopdf](http://wkhtmltopdf.org) render HTML into PDF 

Ichabod is free software/open source, and is distributed under the [MIT](http://opensource.org/licenses/MIT).

Ichabod was created, and is maintained, by:

- [Dave Berton](http://mosey.org)
- [Eric Heydenberk](http://heydenberk.com)

Thanks to support from:

- [Monetate](http://engineering.monetate.com)

