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

import java.awt.Color;
import java.awt.Dimension;
import java.awt.Graphics;
import java.awt.Graphics2D;
import java.awt.Point;
import java.awt.RenderingHints;
import java.awt.Shape;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.KeyAdapter;
import java.awt.event.KeyEvent;
import java.awt.event.MouseAdapter;
import java.awt.event.MouseEvent;
import java.awt.event.MouseMotionAdapter;
import java.awt.event.MouseWheelEvent;
import java.awt.event.MouseWheelListener;
import java.awt.geom.Ellipse2D;
import java.util.Collections;
import java.util.Comparator;
import java.util.Hashtable;
import java.util.LinkedList;
import java.util.List;
import java.util.Vector;
import java.util.prefs.Preferences;
import javax.swing.JPanel;
import javax.swing.Timer;

/*
 * This is the JPanel where the drawings are done. It's initialized with a list of routes
 * created from babel's route table. paintComponent() figures where to draw peers and routes
 * and add all shapes to the list shapeList which is finally "converted" into an array.
 * This array is used in the listeners (mouseMotion and mouse listeners) and recreated each time BabelDraw
is updated (several times per second).
 */

public class BabelDraw extends JPanel {

    boolean isDrawn = false;
    double scale = 1, tx, ty, angel = -1.;
    List<Route> route;
    List<Router> finale;
    List<Shape> shapeList = new LinkedList<Shape>();
    Shape[] shapes;
    MainFrame parent;
    List<Route> neighbour;

    public BabelDraw(String name, MainFrame mf) {
        super();
        setName(name);
        route =(List<Route>) mf.panelRoute.rows;
        parent = mf;
        this.isDrawn = false;
        setFocusable(true);
      

        this.neighbour = setRouter();
        new Timer(100, new ActionListener() {

            public void actionPerformed(ActionEvent arg0) {
                parent.updateLists();
                if (parent.isLinear) {
                    updateSprings();
                } else {
                    updateSpringsWithAccelerator();
                }

                BabelDraw.this.updateUI();
            }
        }).start();

        setBackground(Color.BLACK);
        ActionListener zoom = new ActionListener() {

            public void actionPerformed(ActionEvent e) {
                if (e.getSource() == parent.zoomIn) {
                    scale += 0.2;
                }

                if (e.getSource() == parent.zoomOut) {
                    scale -= 0.2;
                }

                if (scale < 0) {
                    scale = 0;
                }

                shapeList.clear();
                updateUI();
            }
        };

        parent.zoomIn.addActionListener(zoom);
        parent.zoomOut.addActionListener(zoom);

        addMouseWheelListener(new MouseWheelListener() {

            public void mouseWheelMoved(MouseWheelEvent mwe) {
                int clics = mwe.getWheelRotation();
                if (clics == 0) {
                    return;
                }

                if (clics < 0) {
                    clics *= -1;
                    scale += 0.1 * clics;

                } else {
                    scale -= 0.1 * clics;
                }

                if (scale < 0) {
                    scale = 0;
                }
            }
        });

        addMouseListener(new MouseAdapter() {

            @Override
            public void mouseClicked(MouseEvent e) {

                if (!hasFocus()) {
                    requestFocus();
                }

                if (e.getClickCount() == 2) {

                    e.translatePoint((int) -getWidth() / 2, (int) -getHeight() / 2);
                    e.translatePoint((int) -tx, (int) -ty);

                    for (int i = 0; i < shapes.length; i++) {

                        Shape s = shapes[i];

                        if (s instanceof Router && s.contains(e.getPoint())) {

                            Router ps = (Router) s;
                            NameAndColorDialog d = new NameAndColorDialog(parent, isDrawn,
                                    "Change Name and Color",
                                    (String) Preferences.userRoot().get(ps.id, ps.id),
                                    ps.oldC,
                                    "ME");

                            d.setVisible(true);
                            String newName = d.newName;
                            ps.oldC = d.color;
                            ps.c = d.color;
                            d = null;
                            Preferences.userRoot().put(ps.id, newName);
                            repaint();
                            break;

                        }

                    }
                }
            }
        });

        addMouseMotionListener(new MouseMotionAdapter() {

            @Override
            public void mouseMoved(MouseEvent e) {
                if (!hasFocus()) {
                    requestFocus();
                }

                Dimension d = BabelDraw.this.getSize();
                Graphics2D gg = (Graphics2D) getGraphics();
                gg.translate((int) getWidth() / 2, (int) getHeight() / 2);
                e.translatePoint((int) -getWidth() / 2, (int) -getHeight() / 2);
                e.translatePoint((int) -tx, (int) -ty);
                gg.translate(tx, ty);


                // The mouse event translation is a bit weird but its an adjustment due to the translation
                // for the shapes (the center of panel is (0,0), not (getWidth()/2, getHeight()/2)
                for (Shape s : shapes) {
                    if (s instanceof Router) {
                        Router r = (Router) s;

                        if (s.contains(e.getPoint())) {
                            if (r.c != Color.YELLOW) {
                                r.oldC = r.c;
                            }
                            r.c = Color.YELLOW;
                            gg.setColor(r.c);
                            gg.fill(s);

                            parent.infoRouter.setRouter(((Router) s).id);

                        }

                        if (!s.contains(e.getPoint())) {

                            r.c = r.oldC;
                            gg.setColor(r.c);
                            gg.fill(s);

                        }
                    }

                }

            }
        });


        addKeyListener(new KeyAdapter() {

            @Override
            public void keyPressed(KeyEvent e) {
                if (e.getKeyCode() == KeyEvent.VK_UP) {
                    ty += 15.0;
                } else if (e.getKeyCode() == KeyEvent.VK_DOWN) {
                    ty += -15.0;
                } else if (e.getKeyCode() == KeyEvent.VK_RIGHT) {
                    tx += -15.0;
                } else if (e.getKeyCode() == KeyEvent.VK_LEFT) {
                    tx += 15.0;
                } else if (e.getKeyCode() == KeyEvent.VK_PAGE_DOWN) {
                    scale -= 0.2;
                } else if (e.getKeyCode() == KeyEvent.VK_PAGE_UP) {
                    scale += 0.2;
                } else {

                }

                if (scale < 0) {
                    scale = 0;
                }

                updateUI();
            }
        });

        parent.randomize.addActionListener(new ActionListener() {

            public void actionPerformed(ActionEvent arg0) {
                if (finale != null) {
                    synchronized (finale) {
                        for (Router r : finale) {
                            r.angle = Math.random() * 2 * Math.PI;
                            r.velocity = 0.0;
                        }
                    }
                }
            }
        });

        parent.jRadioButton1.addActionListener(new ActionListener() {
            public void actionPerformed(ActionEvent arg0) {
                if (finale != null) {
                    synchronized (finale) {
                        for (Router r : finale) {
                            r.angle = Math.random() * 2 * Math.PI;
                            r.velocity = 0.0;
                        }
                    }
                }
            }
        });
        
        parent.jRadioButton2.addActionListener(new ActionListener() {
            public void actionPerformed(ActionEvent arg0) {
                if (finale != null) {
                    synchronized (finale) {
                        for (Router r : finale) {
                            r.angle = Math.random() * 2 * Math.PI;
                            r.velocity = 0.0;
                        }
                    }
                }
            }
        });
    }

    public boolean isNeighbour(String id) {
        for (Route r : route) {
            if (r.otherValues.get("id").equals(id)) {
                if (r.otherValues.get("refmetric").equals("0")) {
                    return true;
                }
            }
        }
        return false;
    }

    public boolean isMyNeighbour(Route rt, Route neigh) {
        for (Route r : this.route) {
            if (((String) r.otherValues.get("via")).equals(rt.otherValues.get("via"))) {
                if (r.otherValues.get("id").equals(neigh.otherValues.get("id"))) {
                    return true;
                }
            }

        }
        return false;
    }

    public List<Route> setRouter() {
        List<Route> result = new Vector<Route>();
        for (Route r : this.route) {
            if (r.otherValues.get("refmetric").equals("0")) {
                result.add(r);
            }
        }
        return result;
    }

    public double alkashi(double a, double b, double c, String id) {
        //Return the angle formed with lines of length b and c
        double calcule = (-1 * c * c + a * a + b * b) / (2 * a * b);
        if (calcule > 1 || calcule < -1) {
            return 0;
        }

        return Math.acos(calcule);

    }

    public boolean isIpv6Gateway(String id) {

        for (Route r : route) {


            if (r.otherValues.get("prefix").equals("::/0")) {
                if (r.otherValues.get("installed").equals("yes")) {
                    return true;
                }
            }
        }
        return false;
    }

    public boolean isIpv4Gateway(String id) {
        for (Route r : route) {
            if (r.otherValues.get("prefix").equals("0.0.0.0/0")) {
                if (r.otherValues.get("installed").equals("yes")) {
                    return true;
                }
            }
        }
        return false;
    }

    @Override
    public void paintComponent(Graphics g) {
        boolean drawPeers, drawCircles, drawRoutes, drawNames;
        drawCircles = parent.circlesChkBx.isSelected();
        drawPeers = parent.peersChkBx.isSelected();
        drawRoutes = parent.routesChkBx.isSelected();
        drawNames = parent.namesChkBx.isSelected();
        super.paintComponent(g);
        Graphics2D gg = (Graphics2D) g;
        gg.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_ON);
        gg.setColor(Color.WHITE);
        Dimension d = getSize();

        gg.translate(d.width / 2, d.height / 2);// Panel's center is 0,0
        gg.translate(tx, ty);// Apply the current translation

        Point center = new Point(0, 0);

        Collections.sort(route, new Comparator<Route>() {

            public int compare(Route a, Route b) {
                int res = ((String) a.otherValues.get("id")).compareTo((String) b.otherValues.get("id"));
                if (res == 0) {
                    return Integer.parseInt((String) a.otherValues.get("metric")) - Integer.parseInt((String) b.otherValues.get("metric"));
                }
                return res;
            }
        });
        if (!this.isDrawn) {
            shapeList.clear();
        }
        List<Router> lastFinal = new LinkedList<Router>();
        if (this.isDrawn) {
            lastFinal = this.finale;
        }
        List<Router> temp = new LinkedList<Router>();
        double theta = (Math.random() * 1000) % Math.PI;
        int radius = -1;
        for (Route r : route) {
            theta = (Math.random() * 1000) % Math.PI;
            radius = Integer.parseInt((String) r.otherValues.get("metric"));
            if (radius >= 65534) {
                continue;
            }
            temp.add(new Router((int) (scale * radius * Math.cos(theta)), (int) (scale * radius * Math.sin(theta)), r, theta));
        }

        String lastId = null;
        this.finale = new LinkedList<Router>();
        for (Router r : temp) {
            r.x = scale * r.x - Router.radius;
            r.y = scale * r.y - Router.radius;
            if (!r.id.equals(lastId)) {
                finale.add(r);
            }

            lastId = r.id;
        }
        if (this.isDrawn == false) {
            finale.add(new Router((int) (scale * center.x), (int) (scale * center.y), new Route("Me", new Hashtable()), 0));
        }
        if (this.isDrawn) {
            int cmpt = 0;
            for (int i = 0; i < this.finale.size(); i++) {

                int res = this.finale.get(i).id.compareTo(lastFinal.get(cmpt).id);
                radius = 0;
                if (res == 0) {
                    radius = Integer.parseInt((String) this.finale.get(i).route.otherValues.get("metric"));
                    int lastRadius = Integer.parseInt((String) lastFinal.get(cmpt).route.otherValues.get("metric"));
                    if (radius == lastRadius) {
                        this.finale.remove(i);
                        this.finale.add(i, lastFinal.get(cmpt));
                    } else {
                        this.finale.get(i).angle = lastFinal.get(cmpt).angle;
                        this.finale.get(i).x = (int) (scale * radius * Math.cos(this.finale.get(i).angle));
                        this.finale.get(i).y = (int) (scale * radius * Math.sin(this.finale.get(i).angle));
                    }
                }
                if (res < 0) {
                    continue;
                }
                if (res > 0) {
                    i--;
                }
                cmpt++;
            }

            finale.add(lastFinal.get(lastFinal.size() - 1));
            shapeList.clear();
        }
        List<Router> neigh = new LinkedList<Router>();
        for (Router r : this.finale) {
            if (r.id.equals("Me")) {
                continue;
            }
            if (r.route.otherValues.get("refmetric") == null) {
                continue;
            }
            if (r.route.otherValues.get("refmetric").equals("0")) {
                neigh.add(r);
            }
        }

        for (Router r : neigh) {
            for (Router rt : this.finale) {
                if (r.id.equals("Me") || rt.id.equals("Me")) {
                    continue;
                }
                if (rt.route.otherValues.get("refmetric").equals("0")) {
                    continue;
                }
                if (isNeighbour(r.id) && !this.isDrawn) {
                    shapeList.add(new LineShape((String) rt.route.otherValues.get("id"), r.x + Router.radius, r.y + Router.radius, rt.x + Router.radius, rt.y + Router.radius));
                }
                if (isMyNeighbour(r.route, rt.route) && this.isDrawn) {
                    shapeList.add(new LineShape((String) rt.route.otherValues.get("id"), r.x + Router.radius, r.y + Router.radius, rt.x + Router.radius, rt.y + Router.radius));
                }
            }
        }

        for (Router r : finale) {
            if (r.id.equals("Me")) {
                if (drawPeers && this.isDrawn) {
                    shapeList.add(r);

                }
                continue;
            }
            radius = Integer.parseInt((String) r.route.otherValues.get("metric"));
            r.x = (int) (scale * (radius * Math.cos(r.angle)) - Router.radius);
            r.y = (int) (scale * (radius * Math.sin(r.angle)) - Router.radius);
            if (drawCircles) {
                shapeList.add(new Ellipse2D.Double(center.x - scale * radius, center.y - scale * radius, scale * radius * 2, scale * radius * 2));
            }

            if (drawRoutes) {
                if (r.route.otherValues.get("refmetric").equals("0")) {
                    shapeList.add(new LineShape((String) r.route.otherValues.get("id"), center.x, center.y, r.x + Router.radius, r.y + Router.radius));
                }
            }
            if (drawPeers) {
                shapeList.add(r);
            }

        }


        shapes = shapeList.toArray(new Shape[0]); // Stores all shapes in an array
        //(faster than anything else)
        gg.setColor(Color.WHITE);
        for (Shape s : shapes) {

            if (s instanceof Router) {
                if (drawPeers) {
                    gg.setColor(((Router) s).c);
                    gg.fill(s);
                    gg.setColor(Color.WHITE);
                }

                Preferences pref = Preferences.userRoot();
                Router pshap = (Router) s;
                if (drawNames) {

                    gg.setColor(Color.CYAN);

                    gg.drawString((String) pref.get(pshap.id, pshap.id), (float) pshap.getX(), (float) pshap.getY() + 30);
                    gg.setColor(Color.WHITE);

                }

            } else if (s instanceof LineShape) {
                if (isIpv6Gateway(((LineShape) s).id)) {
                    gg.setColor(Color.YELLOW);
                }

                if (isIpv4Gateway(((LineShape) s).id)) {
                    gg.setColor(Color.BLUE);
                }

                if (isIpv6Gateway(((LineShape) s).id) && isIpv4Gateway(((LineShape) s).id)) {
                    gg.setColor(Color.GREEN);
                }

                gg.draw(s);
                gg.setColor(Color.WHITE);

            } else {
                gg.draw(s);

            }

        }
        this.isDrawn = true;
    }

    public int findRefMetric(String id, Route neigh) {
        int res = -1, tmp;
        for (int i = 0; i < route.size(); i++) {
            if (route.get(i) instanceof Route) {
                Route a = (Route) route.get(i);
                if (a.otherValues.get("id").equals(id) && isMyNeighbour(a, neigh)) {
                    tmp = Integer.parseInt((String) a.otherValues.get("refmetric"));
                    if (res == -1 || res < tmp) {
                        res = tmp;
                    }
                }
            }
        }

        return res;
    }

    public static double angleDiff(double a, double b) {
        double d = b - a;
        if (d <= -Math.PI) {
            return d + 2 * Math.PI;
        } else if (d > Math.PI) {
            return d - 2 * Math.PI;
        } else {
            return d;
        }
    }

    public void updateSprings() {
        if (finale == null) {
            return;
        }
        for (Shape s1 : finale) {
            Router pshap1 = (Router) s1;
            if (!isNeighbour(pshap1.id)) {
                continue;
            }
            for (Shape s2 : finale) {
                if (s1 == s2) {
                    continue;
                }
                Router pshap2 = (Router) s2;
                if (pshap2.id.equals("Me")) {
                    continue;
                }

                double drawAngle = angleDiff(pshap1.angle, pshap2.angle + Math.random() / 20);
                double a, b;
                a = Integer.parseInt((String) pshap1.route.otherValues.get("metric"));
                b = Integer.parseInt((String) pshap2.route.otherValues.get("metric"));
                int c1 = findRefMetric(pshap1.id, pshap2.route), c2 = findRefMetric(pshap2.id, pshap1.route);

                int c = 0;

                if (c1 == 0 && c2 == 0) {
                    return;
                } else if (c1 * c2 < 0) {
                    c = -1 * c1 * c2;
                } else if (c1 * c2 == 0) {
                    c = c1 == 0 ? c2 : c1;
                } else {
                    c = (c1 + c2) / 2;
                }
                double realAngle = alkashi(a, b, c, pshap2.id);
                if (realAngle == 0) {
                    continue;
                }
                double diff = angleDiff(drawAngle, realAngle);
                pshap1.angle += diff / 800;
                pshap2.angle -= diff / 800;

            }
        }
   

    }
    
    public void updateSpringsWithAccelerator() {
        if (finale == null) {
            return;
        }
        for (Shape s1 : finale) {
            Router pshap1 = (Router) s1;
            if (!isNeighbour(pshap1.id)) {
                continue;
            }
            for (Shape s2 : finale) {
                if (s1 == s2) {
                    continue;
                }
                Router pshap2 = (Router) s2;
                if (pshap2.id.equals("Me")) {
                    continue;
                }

                double drawAngle = angleDiff(pshap1.angle, pshap2.angle + Math.random() / 20);
                double a, b;
                a = Integer.parseInt((String) pshap1.route.otherValues.get("metric"));
                b = Integer.parseInt((String) pshap2.route.otherValues.get("metric"));
                int c1 = findRefMetric(pshap1.id, pshap2.route), c2 = findRefMetric(pshap2.id, pshap1.route);

                int c = 0;

                if (c1 <= 0 && c2 <= 0) {
                    c = 96;
                } else if (c1 * c2 < 0) {
                    c = -1 * c1 * c2;
                } else if (c1 * c2 == 0) {
                    c = c1 == 0 ? c2 : c1;
                } else {
                    c = (c1 + c2) / 2;
                }
                double realAngle = alkashi(a, b, c, pshap2.id);
                if (realAngle == 0) {
                    continue;
                }
                double diff = angleDiff(drawAngle, realAngle);
                pshap1.velocity *= 0.8; pshap1.velocity += diff / 500;
                pshap2.velocity *= 0.8; pshap2.velocity -= diff / 500;
                pshap1.angle += pshap1.velocity;
                pshap2.angle += pshap2.velocity;
            }
        }
    }

    public static void main(final String[] args) {

        new Thread(new Runnable() {

            public void run() {
                int port = 33123;
                int cmpt = 0;
                for (String s : args) {
                    if (s.equals("-p")) {
                        try {
                            port = Integer.parseInt(args[cmpt + 1]);
                        } catch (Exception e) {
                            port = 33123;
                        }
                    }
                    cmpt++;
                }
                MainFrame mf = new MainFrame("BabelTool", port);
                mf.pack();
                mf.setSize(800, 800);
                mf.setLocationRelativeTo(null);

                mf.setVisible(true);
            }
        }).start();
    }
}
