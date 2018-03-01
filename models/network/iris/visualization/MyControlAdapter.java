package prefuse.demos.applets;

import java.awt.event.MouseEvent;

import prefuse.Display;
import prefuse.controls.ToolTipControl;
import prefuse.visual.VisualItem;

public class MyControlAdapter extends ToolTipControl {
	
    public MyControlAdapter(String field) {
    	super(field);
    	      
    }   

    @Override    
    public void itemEntered(VisualItem item, MouseEvent e) {
        Display d = (Display)e.getSource();
        StringBuilder toolTip = new StringBuilder();
        
        //add router stats and other things in a similar way as below.
        toolTip.append("<html>");
        
        toolTip.append(("<br>Node ID:"));
        toolTip.append(item.getString("name"));
        toolTip.append("</br>");
        toolTip.append(("<br>Avg Pkt Latency: "));
        toolTip.append(item.getString("avg_pkt_latency"));
        toolTip.append("ns</br>");
        toolTip.append(("<br>Last Flit Out Cycle: "));
        toolTip.append(item.getString("last_flit_out_cycle"));
        toolTip.append("</br>");
        toolTip.append(("<br>Flits/Packet: "));
        toolTip.append(item.getString("flits_per_packet"));
        toolTip.append("</br>");
        toolTip.append(("<br>Buffer Occupancy: "));
        toolTip.append(item.getString("buffer_occupancy"));
        toolTip.append("</br>");
        toolTip.append(("<br>SWA Fail Msg Ratio: "));
        toolTip.append(item.getString("swa_fail_msg_ratio"));
        toolTip.append("</br>");
        toolTip.append(("<br>SWA Load: "));
        toolTip.append(item.getString("swa_load"));
        toolTip.append("</br>");
        toolTip.append(("<br>VCA Fail Msg Ratio: "));
        toolTip.append(item.getString("vca_fail_msg_ratio"));
        toolTip.append("</br>");
        toolTip.append(("<br>VCA Load: "));
        toolTip.append(item.getString("vca_load"));
        toolTip.append("</br>");
        toolTip.append(("<br>Stat Pkts: "));
        toolTip.append(item.getString("stat_packets"));
        toolTip.append("</br>");
        toolTip.append(("<br>Stat Flits: "));
        toolTip.append(item.getString("stat_flits"));
        toolTip.append("</br>");
        toolTip.append(("<br>Stat IB Cycles: "));
        toolTip.append(item.getString("stat_ib_cycles"));
        toolTip.append("</br>");
        toolTip.append(("<br>Stat RC Cycles: "));
        toolTip.append(item.getString("stat_rc_cycles"));
        toolTip.append("</br>");
        toolTip.append(("<br>Stat SWA Cycles: "));
        toolTip.append(item.getString("stat_swa_cycles"));
        toolTip.append("</br>");
        toolTip.append(("<br>Stat VCA Cycles: "));
        toolTip.append(item.getString("stat_vca_cycles"));
        toolTip.append("</br>");
        toolTip.append(("<br>Stat ST Cycles: "));
        toolTip.append(item.getString("stat_st_cycles"));
        toolTip.append("</br>");
        toolTip.append("</html>" );
        
        d.setToolTipText(toolTip.toString());
        d.repaint();
    }
    
    /**
     * @see prefuse.controls.Control#itemExited(prefuse.visual.VisualItem, java.awt.event.MouseEvent)
     */
    @Override
    public void itemExited(VisualItem item, MouseEvent e) {
        Display d = (Display)e.getSource();
        d.setToolTipText(null);
        d.repaint();
    }
}