
import java.awt.Color;
import java.awt.event.MouseAdapter;
import java.awt.event.MouseEvent;
import java.awt.event.MouseListener;
import javax.swing.JFrame;
import javax.swing.JLabel;

public class NameAndColorDialog extends NameAndColorDialog_UI {

    public NameAndColorDialog(JFrame parent, boolean modal, String title, String msg, Color c, String newName) {
        super(parent, modal, c, newName);
        setTitle(title);
        this.nameLabel.setText(msg);
        this.currentColor.setBackground(c);
        mainLabel.setText(msg + " :");
        addListeners();
    }

    public void addListeners() {
        MouseListener ml = new MouseAdapter() {

            public void mouseClicked(MouseEvent e) {
                JLabel label = (JLabel) e.getSource();
                Color c = label.getBackground();
                currentColor.setBackground(c);
            }
        };

        green.addMouseListener(ml);
        red.addMouseListener(ml);
        yellow.addMouseListener(ml);
        darkYellow.addMouseListener(ml);
        brown.addMouseListener(ml);
        grey.addMouseListener(ml);
        orange.addMouseListener(ml);
        fushia.addMouseListener(ml);
        cyan.addMouseListener(ml);
        blue.addMouseListener(ml);
        white.addMouseListener(ml);


    }
}
