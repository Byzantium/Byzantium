/*
Copyright (c) 2008 by Pejman Attar and Alexis Rosovsky

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
 */

import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.io.IOException;
import java.net.Socket;
import java.util.Collections;
import java.util.HashMap;
import java.util.Hashtable;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Scanner;
import java.util.Set;

import javax.swing.JOptionPane;
import javax.swing.Timer;


public class BabelReader extends Thread {

    List<Route> route;
    List<XRoute> xroute;
    List<Neighbour> neighbour;
    List<BabelListener> listeners = Collections.synchronizedList(new LinkedList<BabelListener>());
    MainFrame mf;
    static Socket sock;
    Timer tea;
    static int port = 33123;
    String host = "::1";
    public static boolean updateNeigh = false;
    public static boolean updateRoute = false;
    public static boolean updateXRoute = false;

    public BabelReader(int port,MainFrame mf) {
        setName("BabelReader");
        BabelReader.port = port;
        this.mf=mf;
        this.route = Collections.synchronizedList(new LinkedList<Route>());
        this.xroute = Collections.synchronizedList(new LinkedList<XRoute>());
        this.neighbour = Collections.synchronizedList(new LinkedList<Neighbour>());
        tea = new Timer(500, new ActionListener() {

            public void actionPerformed(ActionEvent arg0) {
                if (sock == null) {
                    return;
                }
                if (sock.isClosed()) {

                    JOptionPane.showMessageDialog(null, "Connection error : babel is unreachable.", "Can't connect to " + host + " " + BabelReader.this.port, JOptionPane.ERROR_MESSAGE);

                    tea.stop();
                }

            }
        });
        tea.start();
    }

    public void addBabelListener(BabelListener bl) {
        listeners.add(bl);
    }

    public BabelListener removeBabelListener(BabelListener bl) {
        if (listeners.contains(bl)) {
            listeners.remove(bl);
            return bl;
        }
        return null;
    }

    private void notifyListenersAddRoute(Route r) {
        synchronized (listeners) {
            BabelEvent be = new BabelEvent(this, r);
            for (BabelListener bl : listeners) {
                bl.somethingHappening(be);
                bl.addRoute(be);
            }
            BabelReader.updateRoute = true;
        }
    }

    private void notifyListenersChangeRoute(Route r) {
        synchronized (listeners) {
            BabelEvent be = new BabelEvent(this, r);
            for (BabelListener bl : listeners) {
                bl.somethingHappening(be);
                bl.changeRoute(be);
            }
        }
    }

    private void notifyListenersFlushRoute(Route r) {
        synchronized (listeners) {
            BabelEvent be = new BabelEvent(this, r);
            for (BabelListener bl : listeners) {
                 bl.somethingHappening(be);
                bl.flushRoute(be);
            }
            BabelReader.updateRoute = true;
        }
    }

    private void notifyListenersAddXRoute(XRoute xr) {
        synchronized (listeners) {
            BabelEvent be = new BabelEvent(this, xr);
            for (BabelListener bl : listeners) {
                 bl.somethingHappening(be);
                bl.addXRoute(be);
            }
            BabelReader.updateXRoute = true;
        }
    }

    private void notifyListenersChangeXRoute(XRoute xr) {
        synchronized (listeners) {
            BabelEvent be = new BabelEvent(this, xr);
            for (BabelListener bl : listeners) {
                 bl.somethingHappening(be);
                bl.changeXRoute(be);
            }
        }
    }

    private void notifyListenersFlushXRoute(XRoute xr) {
        synchronized (listeners) {
            BabelEvent be = new BabelEvent(this, xr);
            for (BabelListener bl : listeners) {
                 bl.somethingHappening(be);
                bl.flushXRoute(be);
            }
            BabelReader.updateXRoute = true;
        }
    }

    private void notifyListenersAddNeigh(Neighbour n) {
        synchronized (listeners) {
            BabelEvent be = new BabelEvent(this, n);
            for (BabelListener bl : listeners) {
                 bl.somethingHappening(be);
                bl.addNeigh(be);
            }
            BabelReader.updateNeigh = true;
        }
    }

    private void notifyListenersChangeNeigh(Neighbour n) {
        synchronized (listeners) {
            BabelEvent be = new BabelEvent(this, n);
            for (BabelListener bl : listeners) {
                 bl.somethingHappening(be);
                bl.changeNeigh(be);
            }
        }
    }

    private void notifyListenersFlushNeigh(Neighbour n) {
        synchronized (listeners) {
            BabelEvent be = new BabelEvent(this, n);
            for (BabelListener bl : listeners) {
                 bl.somethingHappening(be);
                bl.flushNeigh(be);
            }
            BabelReader.updateNeigh = true;
        }
    }

    public Route getRoute(String id) {
        synchronized (route) {
            int i = route.indexOf(new Route(id,null));
            if (i == -1) {
                return null;
            } else {
                return route.get(i);
            }
        }
    }

    public XRoute getXRoute(String id) {
        synchronized (xroute) {
            int i = xroute.indexOf(new XRoute(id, null));
            if (i == -1) {
                return null;
            } else {
                return xroute.get(i);
            }
        }

    }

    public Neighbour getNeigh(String id) {
        synchronized (neighbour) {
            int i = neighbour.indexOf(new Neighbour(id,null));
            if (i == -1) {
                return null;
            } else {
                return neighbour.get(i);
            }
        }


    }

    private void addRoute(LexUnit l) throws ParseError {
        HashMap<String, String> map = l.list.map;
        Hashtable more = new Hashtable();


        Set<Map.Entry<String, String>> s = map.entrySet();
        String key = null, value = null;
        for (Map.Entry<String, String> entry : s) {
            key = entry.getKey();

            value = entry.getValue();
            more.put(key, value);
        }

        Route r = new Route(l.id, more);
        this.route.add(r);
        notifyListenersAddRoute(r);
    }

    private void changeRoute(LexUnit l) throws ParseError {
        Route r;
        HashMap<String, String> map = l.list.map;
        if ((r = getRoute(l.id)) == null) {
            throw new ParseError("Id not found in route table : "+l.id);
        }

        Hashtable more = r.otherValues;

        Set<Map.Entry<String, String>> s = map.entrySet();
        String key = null, value = null;
        for (Map.Entry<String, String> entry : s) {
            key = entry.getKey();

            value = entry.getValue();
            if (more.contains(key)) {
                more.remove(key);
            }
            more.put(key, value);

        }
        notifyListenersChangeRoute(r);
    }

    private void flushRoute(LexUnit l) throws ParseError {
        Route r = getRoute(l.id);
        if (r == null) {
            throw new ParseError("Id not found in route table : "+l.id);
        }
        r.position=route.indexOf(r);
          notifyListenersFlushRoute(r);
        this.route.remove(r);
    }

    private void addXRoute(LexUnit l) throws ParseError {
        HashMap<String, String> map = l.list.map;


        String id = l.id;
        Hashtable more = new Hashtable();

        Set<Map.Entry<String, String>> s = map.entrySet();
        String key = null, value = null;
        for (Map.Entry<String, String> entry : s) {
            key = entry.getKey();

            value = entry.getValue();
            more.put(key, value);
        }


        XRoute xr = new XRoute(id, more);
        this.xroute.add(xr);
        notifyListenersAddXRoute(xr);

    }

    private void changeXRoute(LexUnit l) throws ParseError {
        XRoute xr;
        HashMap<String, String> map = l.list.map;
        if ((xr = getXRoute(l.id)) == null) {
            throw new ParseError("Id not found in xroute table : "+l.id);
        }


        Hashtable more = xr.otherValues;

        Set<Map.Entry<String, String>> s = map.entrySet();
        String key = null, value = null;
        for (Map.Entry<String, String> entry : s) {
            key = entry.getKey();

            value = entry.getValue();
            if (more.contains(key)) {
                more.remove(key);
            }
            more.put(key, value);

        }
        notifyListenersChangeXRoute(xr);
    }

    private void flushXRoute(LexUnit l) throws ParseError {
        XRoute xr = getXRoute(l.id);
        if (xr == null) {
            throw new ParseError("Id not found in xroute table : "+l.id);
        }
        xr.position=xroute.indexOf(xr);
        notifyListenersFlushXRoute(xr);
        this.xroute.remove(xr);
    }

    private void addNeighbour(LexUnit l) throws ParseError {
        HashMap<String, String> map = l.list.map;

        Hashtable more = new Hashtable();

        Set<Map.Entry<String, String>> s = map.entrySet();
        String key = null, value = null;
        for (Map.Entry<String, String> entry : s) {
            key = entry.getKey();

            value = entry.getValue();


            more.put(key, value);

        }
        Neighbour n = new Neighbour(l.id, more);
        this.neighbour.add(n);
        notifyListenersAddNeigh(n);
    }

    private void changeNeighbour(LexUnit l) throws ParseError {
        Neighbour n;
        HashMap<String, String> map = l.list.map;
        if ((n = getNeigh(l.id)) == null) {
            throw new ParseError("Id not found in neigh table : "+l.id);
        }


        Hashtable more = n.otherValues;

        Set<Map.Entry<String, String>> s = map.entrySet();
        String key = null, value = null;
        for (Map.Entry<String, String> entry : s) {
            key = entry.getKey();

            value = entry.getValue();
            if (more.contains(key)) {
                more.remove(key);
            }
            more.put(key, value);

        }
        notifyListenersChangeNeigh(n);
    }

    private void flushNeighbour(LexUnit l) throws ParseError {
        Neighbour n = getNeigh(l.id);
        if (n == null) {
            throw new ParseError("Id not found in neigh table : "+l.id);
        }
        n.position=neighbour.indexOf(n);
         notifyListenersFlushNeigh(n);
        this.neighbour.remove(n);
    }

    public void parsing(String s) {
        try {
            String[] t = s.split(" ");
            if (t.length <= 4) {
                if (t[0].equals("BABEL")) {
                    if (t[1].charAt(0) != '0' && t[1].charAt(1) != '.') {
                        throw new LexError("List is even (and should be odd)");
                    } else {
                        return;
                    }
                } else {
                    throw new LexError("Lexing Error problem:" + t[0]);
                }
            } else if ((t.length % 2) == 0) {
                throw new LexError("List is even (and should be odd)");
            }
            LexUnit res = new LexUnit(t);
            if (res.action.action.equals("add")) {
                if (res.object.object.equals("route")) {
                    this.addRoute(res);
                } else if (res.object.object.equals("xroute")) {
                    this.addXRoute(res);
                } else if (res.object.object.equals("neighbour")) {
                    this.addNeighbour(res);
                }
            } else if (res.action.action.equals("change")) {
                if (res.object.object.equals("route")) {
                    this.changeRoute(res);
                } else if (res.object.object.equals("xroute")) {
                    this.changeXRoute(res);
                }/* else if (res.object.object.equals("neighbour")) {
            this.changeNeighbour(res);
            }*/
            } else if (res.action.action.equals("flush")) {
                if (res.object.object.equals("route")) {
                    this.flushRoute(res);
                } else if (res.object.object.equals("xroute")) {
                    this.flushXRoute(res);
                } /*else if (res.object.object.equals("neighbour")) {
            this.flushNeighbour(res);
            }*/
            }

        } catch (Error e) {
            e.printStackTrace();
            System.err.println("Error when parsing : "+s);
        }
    }

    @Override
    public void run() {
        Socket s = null;
        try {
            s = new Socket(host, port);
            sock = s;
            Scanner sc = new Scanner(s.getInputStream());


            while (sc.hasNextLine()) {
                String line = sc.nextLine();
                this.parsing(line);
            }
        } catch (IOException e) {
            JOptionPane.showMessageDialog(null, "Connection error : babel is unreachable.", "Can't connect to " + host + " " + port, JOptionPane.ERROR_MESSAGE);
        } catch (Exception e) {
            System.err.println(e);
            e.printStackTrace();

        } finally {
            try {
                if (s == null) {
                    return;
                }
                s.close();
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
    }
}
