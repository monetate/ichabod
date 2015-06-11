---
layout: doc
title: Benchmarks
category: doc
order: 20
---

## Timing raw HTML rasterization

Sending HTML directly to Ichabod (as opposed to sending it a URL) is
the fastest way to rasterize images. An ipython notebook is used to
create timings.

    %%timeit
    result = client.rasterize(html='<h1>Hello, world!</h1>', width=105)
    
    100 loops, best of 3: 5.11 ms per loop


More complicated HTML:

    %%timeit
    result = client.rasterize(html="""<html>
      <head>
      </head>
      <body>
              <svg version="1.1" id="Layer_1" xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" x="0px" y="0px"
                    viewBox="0 0 19 22" enable-background="new 0 0 19 22" xml:space="preserve">
                    <path fill="#298FCE" d="M18.3,14.8c-0.9,2.1-1.9,2-1.9,2c-2.1,0-2.2-3-2.2-3.4v0c0-0.4,0.1-3.4,2.2-3.4c0,0,1-0.1,1.9,2
                    c0,0,0.3,0.2,0.7,0.1V8.2c0-1.5-1.3-2.7-2.9-2.7H12c0,0,0,0,0,0c-1.6-0.4-0.8-1.3-0.8-1.3c2.5-0.9,2.4-1.9,2.4-1.9
                    c0-2.1-3.5-2.2-4-2.2h0c-0.5,0-4,0.1-4,2.2c0,0-0.1,1,2.4,1.9c0,0,0.8,0.9-0.8,1.3c0,0,0,0,0,0H2.9C1.3,5.5,0,6.7,0,8.2V12
                    c0.4,0.3,0.8,0,0.8,0c0.9-2.1,1.9-2,1.9-2c2.1,0,2.2,3,2.2,3.4v0c0,0.4-0.1,3.4-2.2,3.4c0,0-1,0.1-1.9-2c0,0-0.4-0.3-0.8,0v4.5
                    C0,20.8,1.3,22,2.9,22h5.3c0.2-0.3-0.1-0.7-0.1-0.7c-2.1-0.8-2-1.5-2-1.5C6,18,9.1,18,9.5,18h0C9.9,18,13,18,13,19.9
                    c0,0,0.1,0.7-2,1.5c0,0-0.3,0.3-0.1,0.7h5.3c1.6,0,2.9-1.2,2.9-2.7v-4.6C18.6,14.6,18.3,14.8,18.3,14.8z"/>
              </svg>
      </body>
      </html>""", width=105)

    100 loops, best of 3: 4.82 ms per loop



Even more complicated HTML:

    %%timeit
    result = client.rasterize(html="""<html>
      <head>
      </head>
      <body>
      <style>
    #example1 {
        background-color:#eee;
        box-shadow: 1px 2px 3px 4px black;
        border-radius: 50%;
        background: radial-gradient(circle closest-corner, white, black);
        padding: 20px; 
        margin-top: 10px;
        margin-right: 20px; 
        text-align: center;
    }
    </style>
    <div id="example1">WOAH</div>
      </body>
      </html>""", width=105)

    100 loops, best of 3: 8.5 ms per loop


List elements:

    %%timeit
    result = client.rasterize(html="""<html><body>
    <div class=”error”>
    <h4>My Module</h4>
    <p><strong>Error:</strong>Description of the error…</p>
    <h5>Corrective action required:</h5>
    <ol>
    <li>Step one</li>
    <li>Step two</li>
    </ol>
    </div>
    </body></html>""", width=105)

    100 loops, best of 3: 7.73 ms per loop





  
