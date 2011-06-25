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

import java.net.Inet4Address;
import java.net.Inet6Address;
import java.net.InetAddress;

/*
 * To change this template, choose Tools | Templates
 * and open the template in the editor.
 */
/**
 *
 * @author momotte
 */
public class InetAddressPrefix {

    private String ip;
    private int prefix;

    public InetAddressPrefix(String ip_pref) throws LexError {
        String[] t = ip_pref.split("/");
        if (t.length != 2) {
            throw new LexError("LexError : IP in InetAddressPrefix " + "-> " + ip_pref);
        }
        prefix = Integer.parseInt(t[1]);
        ip = t[0];
    }

    public boolean isIPv4() {
        try {
            InetAddress ina = InetAddress.getByName(ip);
            return (ina instanceof Inet4Address);
        } catch (Exception e) {
            e.printStackTrace();
        }
        return false;
    }

    public boolean isIPv6() {
        try {
            InetAddress ina = InetAddress.getByName(ip);
            return (ina instanceof Inet6Address);
        } catch (Exception e) {
            e.printStackTrace();
        }
        return false;
    }

    public boolean isRange() {
        if (isIPv4()) {
            return prefix == 32;
        } else {
            return prefix == 128;
        }
    }

    public long range() {
        if (isIPv4()) {
            return (long) Math.pow(2, 32 - prefix);
        } else {
            return (long) Math.pow(2, 128 - prefix);
        }
    }
}
