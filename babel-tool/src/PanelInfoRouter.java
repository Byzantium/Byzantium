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

import java.util.Collections;
import java.util.LinkedList;
import java.util.List;
import java.util.prefs.Preferences;
import javax.swing.DefaultListModel;
import javax.swing.ListSelectionModel;
import javax.swing.event.ListSelectionEvent;
import javax.swing.event.ListSelectionListener;


public class PanelInfoRouter extends PanelInfoRouter_UI {

    MainFrame mf;
    List routes = Collections.synchronizedList(new LinkedList());

    public PanelInfoRouter(MainFrame mf) {
        super();
        this.mf = mf;
        listRoute.setSelectionMode(ListSelectionModel.SINGLE_SELECTION);

        listRoute.setModel(new DefaultListModel());
        listRoute.addListSelectionListener(new ListSelectionListener() {

            public void valueChanged(ListSelectionEvent e) {
                int i = listRoute.getSelectedIndex();
                if (i < 0) {
                    return;
                }
                Route r = (Route) routes.get(i);

                routeID.setText(r.id);

                installed.setText((String) r.otherValues.get("installed"));

                sourceID.setText((String) r.otherValues.get("id"));

                metric.setText((String) r.otherValues.get("metric"));

                refmetric.setText((String) r.otherValues.get("refmetric"));

                via.setText((String) r.otherValues.get("via"));

                interfRoute.setText((String) r.otherValues.get("if"));

                prefix.setText((String) r.otherValues.get("prefix"));
            }
        });
    }

    public void clear() {
        String d = "Default";
        id.setText(d);
        name.setText(d);
        neighID.setText(d);
        reach.setText(d);
        cost.setText(d);
        rxcost.setText(d);
        txcost.setText(d);
        address.setText(d);
        interf.setText(d);

        installed.setText(d);
        routeID.setText(d);
        metric.setText(d);
        refmetric.setText(d);
        prefix.setText(d);
        via.setText(d);
        sourceID.setText(d);
        interfRoute.setText(d);
    }

    public void setRouter(final String ID) {
        clear();
        if (!routes.isEmpty()) {
            routes.clear();
            ((DefaultListModel) listRoute.getModel()).clear();
        }
        String s = "Unknown";
        id.setText(ID);
        name.setText(Preferences.userRoot().get(ID, ID));
        if (ID.equals("Me")) {
            neighID.setText("alamakota");
            reach.setText(s);
            cost.setText(s);
            rxcost.setText(s);
            txcost.setText(s);
            address.setText(s);
            interf.setText(s);

            routeID.setText(s);
            metric.setText(s);
            refmetric.setText(s);
            prefix.setText(s);
            via.setText(s);
            sourceID.setText(s);
            interfRoute.setText(s);
            return;
        }
        synchronized (mf.panelNeigh.rows) {
            String ns = null, is = null;
            boolean ambiguous = false, ambiguous_if = false;
            List<Route> routeList = (List<Route>) mf.panelRoute.rows;
            for (Route r : routeList) {
                if (((String) r.otherValues.get("id")).equals(ID)) {
                    if(r.otherValues.get("refmetric").equals("0")) {
                        String n, i;
                        n = (String)r.otherValues.get("via");
                        i = (String)r.otherValues.get("if");
                        if(i != null) {
                            if(is == null)
                                is = i;
                            else if(!is.equals(i))
                                ambiguous_if = true;
                        }

                        if(n != null) {
                            if(ns == null)
                                ns = n;
                            else if(!ns.equals(n))
                                ambiguous = true;
                        }
                    }
                }
            }

            Neighbour neigh = null;
            if(ns != null && is != null && !ambiguous && !ambiguous_if) {
                List<Neighbour> neighList = (List<Neighbour>)mf.panelNeigh.rows;
                for(Neighbour n : neighList) {
                    if(n.otherValues.get("address").equals(ns) &&
                       n.otherValues.get("if").equals(is)) {
                        neigh = n;
                        break;
                    }
                }
            }

            if(ambiguous) {
                neighID.setText("<html><font color=\"red\">Ambiguous");
            } else if(neigh == null) {
                neighID.setText("<html><font color=\"blue\">Not a neighbour.");
            } else {
                neighID.setText(neigh.id);
                reach.setText((String) neigh.otherValues.get("reach"));
                cost.setText((String) neigh.otherValues.get("cost"));
                rxcost.setText((String) neigh.otherValues.get("rxcost"));
                txcost.setText((String) neigh.otherValues.get("txcost"));
                address.setText((String) neigh.otherValues.get("address"));
            }

            if(!ambiguous_if && is != null)
                interf.setText(is);
            else
                interf.setText("(unknown)");
        }


        LinkedList<Route> mine = new LinkedList<Route>();
        synchronized (mf.panelRoute.rows) {
            List<Route> routeList = (List<Route>) mf.panelRoute.rows;
            for (Route r : routeList) {
                if (r.otherValues.get("id").equals(ID)) {
                    mine.add(r);
                }
            }
        }

        DefaultListModel dlm = ((DefaultListModel) (listRoute.getModel()));
        for (Route r : mine) {
            routes.add(r);
            dlm.addElement(r);
        }
        listRoute.setSelectedIndex(0);

    }
}
