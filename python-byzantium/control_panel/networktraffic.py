# networktraffic.py - Web application that displays the rrdtool traffic
#    graphs, so that the node's administrator can see how active their node is.

# Project Byzantium: http://wiki.hacdc.org/index.php/Byzantium
# License: GPLv3

# Import modules.
import os


# Classes.
# This class implements the network traffic status report page.
class NetworkTraffic(object):
    # Pretends to be index.html.
    
    def __init__(self, filedir, templatelookup):
        self.filedir = filedir
        self.templatelookup = templatelookup
    
    def index(self):
        # Enumerate the list of PNG files in the graphs/ directory and generate
        # a sequence of IMG SRCs to insert into the HTML template.
        graphdir = os.path.join(self.filedir,"graphs")
        images = os.listdir(graphdir)

        # Pack the string of IMG SRCs into a string.
        graphs = ""
        for image in images:
            graphs = graphs + '<img src="/graphs/' + image + '" width="75%"' + 'height="75%" alt="' + image + '" /><br />'

        page = self.templatelookup.get_template("/traffic/index.html")
        return page.render(graphs = graphs,
                           title = "Byzantium Network Traffic Report",
                           purpose_of_page = "Traffic Graphs")
    index.exposed = True
