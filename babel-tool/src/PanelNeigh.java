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
import java.util.Hashtable;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Set;
import javax.swing.table.AbstractTableModel;


public class PanelNeigh extends PanelBabel_UI {

    public List<String> ids = null;
    List<Neighbour> neigh = null;
    String[] additionalKeys;

    public PanelNeigh(String name) {
        super();
        setName(name);
        table.getTableHeader().setReorderingAllowed(false);

    }

    public void setTable(Hashtable<String, Neighbour> t) {

        if (ids == null && neigh == null) {

            ids = Collections.synchronizedList(new LinkedList<String>());
            neigh = Collections.synchronizedList(new LinkedList<Neighbour>());
            synchronized (ids) {
                synchronized (neigh) {


                    Set<Map.Entry<String, Neighbour>> s = t.entrySet();

                    for (Map.Entry<String, Neighbour> entry : s) {
                        ids.add(entry.getKey());
                        neigh.add(entry.getValue());

                    }
                    Neighbour def = null;
                    int moreCol = 0;
                    for (int i = 0; i < neigh.size(); i++) {
                        int size = neigh.get(i).otherValues.entrySet().size();
                        if (moreCol < size) {
                            moreCol = size;
                            def = neigh.get(i);
                        }
                    }


                    additionalKeys = new String[moreCol];
                    if (def != null && def.otherValues != null) {
                        Set<Map.Entry<String, String>> set = def.otherValues.entrySet();
                        int i = 0;
                        for (Map.Entry<String, String> entry : set) {
                            additionalKeys[i] = entry.getKey();
                            i++;
                        }
                    }
                }

                table.setModel(new AbstractTableModel() {

                    public int getRowCount() {
                        return ids.size();
                    }

                    public int getColumnCount() {
                        return additionalKeys.length + 1;
                    }

                    public Object getValueAt(int row, int col) {

                        if (col == 0) {
                            return ids.get(row);
                        } else {
                            return neigh.get(row).otherValues.get(this.getColumnName(col));
                        }

                    }

                    @Override
                    public String getColumnName(int n) {
                        if (n == 0) {
                            return "ID";
                        } else {
                            return additionalKeys[n - 1];
                        }

                    }
                });

            }
        } else {
            Set<Map.Entry<String, Neighbour>> s = t.entrySet();
            for (Map.Entry<String, Neighbour> entry : s) {
                if (!neigh.contains(entry.getValue())) {
                    ids.add(entry.getKey());
                    neigh.add(entry.getValue());
                }

            }

            for (String id : ids) {
                if (!t.containsKey(id)) {
                    ids.remove(id);
                    neigh.remove(id);
                }
            }


            ((AbstractTableModel) table.getModel()).fireTableDataChanged();
        }
    }
}
