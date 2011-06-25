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

public class LexUnit {

    LexAction action;
    LexObject object;
    String id;
    LexList list;

    public LexUnit(String[] t) throws LexError {
        action = new LexAction(t[0]);
        object = new LexObject(t[1]);
        id = t[2];
        list = new LexList(t);
    }

    @Override
    public String toString() {
        StringBuffer sb = new StringBuffer();
        sb.append("Action : " + action + " | ");
        sb.append("Object : " + object + " | ");
        sb.append("ID : " + id);
        sb.append("\n");
        sb.append("List : " + list);
        sb.append("\n\n");

        return sb.toString();
    }
}
