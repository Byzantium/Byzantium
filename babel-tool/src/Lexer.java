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

public class Lexer {

    public static String[] lexing(String s) throws LexError {
        String[] tmp = s.split(" ");
        if (tmp.length < 3) {
            if (tmp[0].equals("Trying") || tmp[0].equals("Connected")) {
                ;
            } else if (tmp[0].equals("BABEL")) {
                if (tmp[1].charAt(0) == '0' && tmp[1].charAt(1) == '.') {
                    ;
                } else {
                    System.err.println("Wrong protocole number");
                }
            } else {
                throw new LexError("Lexing Error problem:" + tmp[0]);
            }
        }
        return tmp;
    }
}
