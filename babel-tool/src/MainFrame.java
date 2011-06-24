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

import java.awt.Dimension;
import java.awt.EventQueue;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;


import java.util.Collections;
import java.util.LinkedList;
import java.util.List;
import javax.swing.JSplitPane;
import javax.swing.JTable;

import javax.swing.event.TableModelEvent;
import javax.swing.table.AbstractTableModel;

public class MainFrame extends MainFrame_UI {

    PanelBabel<Neighbour> panelNeigh;
    PanelBabel<Route> panelRoute;
    PanelBabel<XRoute> panelXroute;
    BabelDraw babelDraw = null;
    JSplitPane split = new JSplitPane(JSplitPane.HORIZONTAL_SPLIT);
    PanelInfoRouter infoRouter;
    BabelReader babelReader;

    public MainFrame(String name, int port) {
        super();
        babelReader = new BabelReader(port, this);
        babelReader.start();
        setTitle(name);

        panelNeigh = new PanelBabel<Neighbour>("Neighbours");
        panelRoute = new PanelBabel<Route>("Routes");
        panelXroute = new PanelBabel<XRoute>("XRoutes");

        tabbedPane.add(panelNeigh);
        tabbedPane.add(panelRoute);
        tabbedPane.add(panelXroute);

        initGui();

        EventQueue.invokeLater(new Runnable() {

            public void run() {
                setBabel();
                drawNeighbours();
            }
        });
    }

    public void initGui() {
        ActionListener redraw = new ActionListener() {

            public void actionPerformed(ActionEvent arg0) {
                try {
                    babelDraw.updateUI();
                    if (arg0.getSource() == peersChkBx && !peersChkBx.isSelected()) {
                        namesChkBx.setSelected(false);
                    }
                } catch (Exception e) {
                    e.printStackTrace();
                }
            }
        };
        peersChkBx.addActionListener(redraw);
        circlesChkBx.addActionListener(redraw);
        routesChkBx.addActionListener(redraw);
        namesChkBx.addActionListener(redraw);

    }

    public void drawNeighbours() {
        if (babelDraw == null) {
            babelDraw = new BabelDraw("Babel Draw", this);
            split.setLeftComponent(babelDraw);
            Dimension min = new Dimension(0, 0);
            babelDraw.setMinimumSize(min);
            infoRouter = new PanelInfoRouter(this);
            split.setRightComponent(infoRouter);
            infoRouter.setMinimumSize(min);

            tabbedPane.addTab("BabelDraw", split);
            split.setResizeWeight(0.7);
        } else {
            babelDraw.shapeList.clear();
            babelDraw.updateUI();
        }
    }

    public void setBabel() {
        List<Neighbour> copyNeigh;
        List<Route> copyRoute;
        List<XRoute> copyXroute;
        synchronized (babelReader.neighbour) {
            copyNeigh = new LinkedList();
            copyNeigh.addAll(babelReader.neighbour);
        }
        synchronized (babelReader.route) {
            copyRoute = new LinkedList<Route>();
            copyRoute.addAll(babelReader.route);
        }
        synchronized (babelReader.xroute) {
            copyXroute = new LinkedList<XRoute>();
            copyXroute.addAll(babelReader.xroute);
        }

        panelNeigh.setTable(copyNeigh);
        panelRoute.setTable(copyRoute);
        panelXroute.setTable(copyXroute);

        JTable routeTable, XrouteTable, neighTable;
        routeTable = panelRoute.table;
        XrouteTable = panelXroute.table;
        neighTable = panelNeigh.table;
        new Thread(new Runnable() {

            public void run() {
                //Sometimes, the number of columns isn't properly computed. This happends under particular (and random) conditions.
                //I figure how many columns are needed by analysing one element of the table (Route, Neighbour or XRoute)
                //If setTable is invoked with an empty list, there is no element i can look into, so the column number is set to one.
                //Until columns are computed again, the column number wont change.
                
                //The following thread will run only once, 3 seconds after the initial call of setTable and will recompute the columns.
                //Three seconds is much more time than the BabelReader needs to fill its lists, but it's a simple way
                //to be sure that its lists aren't empty :)
                //Note that if columns are already all there it'll change nothing and that 
                //any update of any table which has only one column will trigger a new column computation.
                try {
                    Thread.sleep(3000);
                } catch (Exception e) {
                    e.printStackTrace();
                } finally {
                    JTable routeTable, XrouteTable, neighTable;
                    routeTable = panelRoute.table;
                    XrouteTable = panelXroute.table;
                    neighTable = panelNeigh.table;
                    ((AbstractTableModel) routeTable.getModel()).fireTableStructureChanged();
                    ((AbstractTableModel) XrouteTable.getModel()).fireTableStructureChanged();
                    ((AbstractTableModel) neighTable.getModel()).fireTableStructureChanged();

                }

            }
        }).start();
        babelReader.addBabelListener(new BabelListener() {

            public void somethingHappening(BabelEvent e) {
                JTable table = null;
                PanelBabel panel = null;
                if (e.item instanceof Route) {
                    table = panelRoute.table;
                    panel = panelRoute;
                } else if (e.item instanceof XRoute) {
                    table = panelXroute.table;
                    panel = panelXroute;
                } else if (e.item instanceof Neighbour) {
                    table = panelNeigh.table;
                    panel = panelNeigh;
                }
                if (panel.rows != null && table.getColumnCount() == 1 && panel.rows.size() != 0) {
                    ((AbstractTableModel) table.getModel()).fireTableChanged(new TableModelEvent(table.getModel(), TableModelEvent.HEADER_ROW));
                }

            }

            public void addRoute(BabelEvent e) {
                synchronized (panelRoute.rows) {
                    int i = panelRoute.rows.indexOf(e.item);
                    ((AbstractTableModel) panelRoute.table.getModel()).fireTableRowsInserted(i, i);
                }
            }

            public void addXRoute(BabelEvent e) {
                synchronized (panelXroute.rows) {
                    int i = panelXroute.rows.indexOf(e.item);
                    ((AbstractTableModel) panelXroute.table.getModel()).fireTableRowsInserted(i, i);
                }
            }

            public void addNeigh(BabelEvent e) {
                synchronized (panelNeigh.rows) {
                    int i = panelNeigh.rows.indexOf(e.item);
                    ((AbstractTableModel) panelNeigh.table.getModel()).fireTableRowsInserted(i, i);
                }
            }

            public void changeRoute(BabelEvent e) {
                synchronized (panelRoute.rows) {
                    int i = panelRoute.rows.indexOf(e.item);
                    int j = ((AbstractTableModel) panelRoute.table.getModel()).getRowCount();
                    if (i < j) {
                    try {
                        ((AbstractTableModel) panelRoute.table.getModel()).fireTableRowsUpdated(i, i);
                    }catch(Exception exn){
                    /* DefaultRowSorter.rowsUpdated() regularly and
                       mysteriously triggers IndexOutOfBoundsException here.
                       I don't know how to fix it, so I ignore it.
                    */
                    }
                    }
                }
            }

            public void changeXRoute(BabelEvent e) {
                synchronized (panelXroute.rows) {
                    int i = panelXroute.rows.indexOf(e.item);
                    ((AbstractTableModel) panelXroute.table.getModel()).fireTableRowsUpdated(i, i);
                }
            }

            public void changeNeigh(BabelEvent e) {
                synchronized (panelNeigh.rows) {
                    int i = panelNeigh.rows.indexOf(e.item);
                    ((AbstractTableModel) panelNeigh.table.getModel()).fireTableRowsUpdated(i, i);
                }
            }

            public void flushRoute(BabelEvent e) {
                synchronized (panelRoute.rows) {
                    int i = e.item.position;
                    ((AbstractTableModel) panelRoute.table.getModel()).fireTableDataChanged();
                }
            }

            public void flushXRoute(BabelEvent e) {
                synchronized (panelXroute.rows) {
                    int i = e.item.position;
                    ((AbstractTableModel) panelXroute.table.getModel()).fireTableDataChanged();
                }
            }

            public void flushNeigh(BabelEvent e) {
                synchronized (panelNeigh.rows) {
                    int i = e.item.position;
                    ((AbstractTableModel) panelNeigh.table.getModel()).fireTableDataChanged();
                }
            }
        });
    }

    public void updateLists() {
        if (BabelReader.updateNeigh) {
            this.panelNeigh.updateRows(babelReader.neighbour);
        }

        if (BabelReader.updateRoute) {
            this.panelRoute.updateRows(babelReader.route);
        }

        if (BabelReader.updateXRoute) {
            this.panelXroute.updateRows(babelReader.xroute);
        }

    }
}
