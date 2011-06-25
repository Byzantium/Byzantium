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

import java.util.List;
import java.util.Map;
import java.util.Set;
import javax.swing.table.AbstractTableModel;

public class PanelBabel<T extends BabelItem> extends PanelBabel_UI {

    List<? extends BabelItem> rows = null;
    String[] additionalKeys;
    int defaultColNumber = 1;

    public PanelBabel(String name) {
        super();
        setName(name);
        table.getTableHeader().setReorderingAllowed(false);



    }

    public void setTable(List<T> t) {

        rows = t;
        table.setModel(new AbstractTableModel() {

            public int getRowCount() {
                return rows.size();
            }

            public int getColumnCount() {
                T def = null;
                int moreCol = 0;
                for (int i = 0; i < rows.size(); i++) {
                    int size = rows.get(i).otherValues.entrySet().size();
                    if (moreCol < size) {
                        moreCol = size;
                        def = (T) rows.get(i);
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
                if (additionalKeys.length != 0) {
                    return additionalKeys.length + 1;
                } else {
                    return defaultColNumber;
                }
            }

            public Object getValueAt(int row, int col) {
                try {
                    if (col == 0) {
                        if (rows.get(row) instanceof Route) {

                            String prefix = "<html><table>";
                            if (rows.get(row).otherValues.get("installed").equals("yes")) {
                                prefix += "<tr><td><b>";
                            } else if (rows.get(row).otherValues.get("prefix").equals("::/0") && rows.get(row).otherValues.get("prefix").equals("0.0.0.0/0")) {
                                prefix += "<tr bgcolor=\"green\"><td>";
                            } else if (rows.get(row).otherValues.get("prefix").equals("::/0")) {
                                prefix += "<tr bgcolor=\"yellow\"><td>";
                            } else if (rows.get(row).otherValues.get("prefix").equals("0.0.0.0/0")) {
                                prefix += "<tr bgcolor=\"blue\"><td>";
                            } else {
                                prefix += "<tr><td>";
                            }
                            return prefix + rows.get(row).id + "</tr></td></table>";

                        } else {
                            return rows.get(row).id;
                        }
                    } else {
                        if (rows.get(row) instanceof Route) {
                            String prefix = "<html><table>";
                            if (rows.get(row).otherValues.get("installed").equals("yes")) {
                                prefix += "<tr><td><b>";
                            } else if (rows.get(row).otherValues.get("prefix").equals("::/0") && rows.get(row).otherValues.get("prefix").equals("0.0.0.0/0")) {
                                prefix += "<tr bgcolor=\"green\"><td>";
                            } else if (rows.get(row).otherValues.get("prefix").equals("::/0")) {
                                prefix += "<tr bgcolor=\"yellow\"><td>";
                            } else if (rows.get(row).otherValues.get("prefix").equals("0.0.0.0/0")) {
                                prefix += "<tr bgcolor=\"blue\"><td>";
                            } else {
                                prefix += "<tr><td>";
                            }
                            return prefix + rows.get(row).otherValues.get(this.getColumnName(col)) + "</tr></td></table>";
                        } else {
                            return rows.get(row).otherValues.get(this.getColumnName(col));
                        }

                    }
                } catch (Exception e) {
                    e.printStackTrace();
                    return null;
                }
            }

            @Override
            public String getColumnName(int n) {
                if (n == 0) {
                    return "ID";
                } else {
                    try {
                        return additionalKeys[n - 1];
                    } catch (ArrayIndexOutOfBoundsException e) {
                        return "wait ...";
                    }
                }

            }
        });

    }

    public void updateRows(List<? extends BabelItem> readerList) {
        synchronized (this.rows) {
            synchronized (readerList) {
                rows.clear();
                List <BabelItem> temp=(List<BabelItem>) rows;
                temp.addAll(readerList);
            }
        }
    }
}



