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

import java.util.EventListener;


public interface BabelListener extends EventListener{

    public void addRoute(BabelEvent e);
    
    public void addXRoute(BabelEvent e);
    
    public void addNeigh(BabelEvent e);
    
    public void changeRoute(BabelEvent e);
    
    public void changeXRoute(BabelEvent e);
    
    public void changeNeigh(BabelEvent e);
    
    public void flushRoute(BabelEvent e);
    
    public void flushXRoute(BabelEvent e);
    
    public void flushNeigh(BabelEvent e);
    
    public void somethingHappening(BabelEvent e);
    //Invoked for all kinds of changes, just before add/change/flush call
    
}
