package prefuse.demos.applets;

import java.awt.Color;
import java.awt.Dimension;
import java.awt.Font;

import javax.swing.BorderFactory;
import javax.swing.Box;
import javax.swing.BoxLayout;
import javax.swing.JComponent;
import javax.swing.JLabel;
import javax.swing.JPanel;
import javax.swing.JSplitPane;
import javax.swing.JTextArea;
import javax.swing.SwingConstants;
import javax.swing.event.ChangeEvent;
import javax.swing.event.ChangeListener;

import prefuse.Constants;
import prefuse.Display;
import prefuse.Visualization;
import prefuse.action.ActionList;
import prefuse.action.RepaintAction;
import prefuse.action.assignment.ColorAction;
import prefuse.action.assignment.DataColorAction;
import prefuse.action.assignment.DataShapeAction;
import prefuse.action.assignment.DataSizeAction;
import prefuse.action.filter.GraphDistanceFilter;
import prefuse.action.layout.SpecifiedLayout;
import prefuse.action.layout.graph.ForceDirectedLayout;
import prefuse.activity.Activity;
import prefuse.controls.DragControl;
import prefuse.controls.FocusControl;
import prefuse.controls.NeighborHighlightControl;
import prefuse.controls.PanControl;
import prefuse.controls.WheelZoomControl;
import prefuse.controls.ZoomControl;
import prefuse.controls.ZoomToFitControl;
import prefuse.data.Graph;
import prefuse.data.Tuple;
import prefuse.data.event.TupleSetListener;
import prefuse.data.io.GraphMLReader;
import prefuse.data.tuple.TupleSet;
import prefuse.render.DefaultRendererFactory;
import prefuse.render.AbstractShapeRenderer;
import prefuse.render.Renderer;
import prefuse.render.RendererFactory;
import prefuse.render.LabelRenderer;
import prefuse.render.ShapeRenderer;
import prefuse.render.EdgeRenderer;
import prefuse.util.ColorLib;
import prefuse.util.FontLib;
import prefuse.util.GraphLib;
import prefuse.util.PrefuseLib;
import prefuse.util.force.ForceSimulator;
import prefuse.util.ui.JFastLabel;
import prefuse.util.ui.JForcePanel;
import prefuse.util.ui.JPrefuseApplet;
import prefuse.util.ui.JValueSlider;
import prefuse.util.ui.UILib;
import prefuse.visual.NodeItem;
import prefuse.visual.VisualGraph;
import prefuse.visual.VisualItem;
import prefuse.visual.expression.InGroupPredicate;

/**
 * @author <a href="http://jheer.org">jeffrey heer</a>
 */
@SuppressWarnings("serial")
public class GraphView extends JPrefuseApplet {

    private static final String graph = "graph";
    private static final String nodes = "graph.nodes";
    private static final String edges = "graph.edges";
    @SuppressWarnings("unused")
	private static final String results = "graph.result";
    
    public void init() {
        UILib.setPlatformLookAndFeel();
        JComponent graphview = demo("/output.graphml", "name");
        this.getContentPane().add(graphview);
    }

    public static JComponent demo(String datafile, String label) {
        Graph g = null;
        if ( datafile == null ) {
            g = GraphLib.getGrid(128,128);
        } else {
            try {
                g = new GraphMLReader().readGraph(datafile);
            } catch ( Exception e ) {
                e.printStackTrace();
                System.exit(1);
            }
        }
        
        return demo(g, label);
    }
    
    public static JComponent demo(Graph g, String label) {

        // create a new, empty visualization for our data
        final Visualization vis = new Visualization();
        VisualGraph vg = vis.addGraph(graph, g);
        vis.setValue(edges, null, VisualItem.INTERACTIVE, Boolean.FALSE);
        int[] palette = new int[] {
                ColorLib.rgb(77, 175, 74),
                ColorLib.rgb(55, 126, 184),
                ColorLib.rgb(0, 0, 0),
                ColorLib.rgb(228, 26, 28),
                ColorLib.rgb(152, 78, 163),
                ColorLib.rgb(255, 127, 0)
        	};

        int[] shape_palette = new int[] {
        		Constants.SHAPE_DIAMOND,
        		Constants.SHAPE_ELLIPSE,
        		Constants.SHAPE_TRIANGLE_UP,
        		Constants.SHAPE_RECTANGLE
        };
        
                               
        
        TupleSet focusGroup = vis.getGroup(Visualization.FOCUS_ITEMS); 
        focusGroup.addTupleSetListener(new TupleSetListener() {
            public void tupleSetChanged(TupleSet ts, Tuple[] add, Tuple[] rem)
            {
                for ( int i=0; i<rem.length; ++i )
                    ((VisualItem)rem[i]).setFixed(false);
                for ( int i=0; i<add.length; ++i ) {
                    ((VisualItem)add[i]).setFixed(false);
                    ((VisualItem)add[i]).setFixed(true);
                }
                vis.run("draw");
            }
        });
        
        // set up the renderers
        
/*      ShapeRenderer s = new ShapeRenderer();
        s.rectangle(0, 0, 30, 30);
        vis.setRendererFactory(new DefaultRendererFactory(s));*/
        
        MyRenderer tr = new MyRenderer(label);
        DefaultRendererFactory d = new DefaultRendererFactory(tr);
        
        MyMultiEdgeRenderer myEdgeRenderer = new MyMultiEdgeRenderer(1, 1);
        //EdgeRenderer myEdgeRenderer = new EdgeRenderer();
        
        d.add(new InGroupPredicate(edges), myEdgeRenderer);
        vis.setRendererFactory(d);
        
        // -- set up the actions ----------------------------------------------
        
        int maxhops = 1024, hops = 1024;
        final GraphDistanceFilter filter = new GraphDistanceFilter(graph, hops);

        ActionList draw = new ActionList();
        draw.add(filter);        
        
        //draw.add(new ColorAction(nodes, VisualItem.FILLCOLOR, ColorLib.rgb(180,180,255)));
        draw.add(new ColorAction(nodes, VisualItem.STROKECOLOR, 0));
        draw.add(new ColorAction(nodes, VisualItem.TEXTCOLOR, ColorLib.rgb(0,0,0)));
        draw.add(new ColorAction(edges, VisualItem.FILLCOLOR, ColorLib.gray(100)));
        draw.add(new ColorAction(edges, VisualItem.STROKECOLOR, ColorLib.gray(170)));
        draw.add(new DataShapeAction(nodes,"type", shape_palette));
        //draw.add(new DataShapeAction(edges,"type", edge_palette));
        
        DataColorAction fill = new DataColorAction("graph.nodes", "type",
        	    Constants.NOMINAL, VisualItem.FILLCOLOR, palette);
        	// use black for node text
    	ColorAction text = new ColorAction("graph.nodes",
    	    VisualItem.TEXTCOLOR, ColorLib.gray(0));
    	// use light grey for edges
    	ColorAction edges = new ColorAction("graph.edges",
    	    VisualItem.STROKECOLOR, ColorLib.gray(200));

    	// create an action list containing all color assignments
    	ActionList color = new ActionList();
    	color.add(fill);
    	color.add(text);
    	color.add(edges);
    	
//        ColorAction fill = new ColorAction(nodes, 
//                VisualItem.FILLCOLOR, ColorLib.rgb(200,200,255));
        fill.add("_fixed", ColorLib.rgb(255,100,100));
        fill.add("_highlight", ColorLib.rgb(255,200,125));
        
        /*ForceDirectedLayout fdl = new ForceDirectedLayout(graph);
        ForceSimulator fsim = fdl.getForceSimulator();
        fsim.getForces()[0].setParameter(0, -5f);
        
        ActionList animate = new ActionList(Activity.INFINITY);
        //ActionList animate = new ActionList(vis);

        animate.add(fdl);
        animate.add(fill);
        animate.add(new RepaintAction()); */
        
        SpecifiedLayout fdl = new SpecifiedLayout ("graph.nodes","x_coord", "y_coord");       
    
        ActionList location = new ActionList(vis);
        location.add(fdl);
        location.add(fill);
        location.add(new RepaintAction());

        DataSizeAction edge_type = new DataSizeAction("graph.edges","type");
        location.add(edge_type);
        //vis.putAction("location", location);

        
        // finally, we register our ActionList with the Visualization.
        // we can later execute our Actions by invoking a method on our
        // Visualization, using the name we've chosen below.
        vis.putAction("draw", draw);
        vis.putAction("layout", location);
        vis.runAfter("draw", "layout");
        vis.putAction("color", color);
        //vis.putAction("layout", layout);       
        
        // --------------------------------------------------------------------
        // STEP 4: set up a display to show the visualization
        
        Display display = new Display(vis);
        display.setSize(2000,2000);
        //display.setForeground(Color.DARK_GRAY);
        //display.setBackground(Color.WHITE);
        display.setForeground(Color.DARK_GRAY);
        display.setBackground(Color.LIGHT_GRAY);
        
        // main display controls
        display.addControlListener(new FocusControl(1));
        display.addControlListener(new DragControl());
        display.addControlListener(new PanControl());
        display.addControlListener(new ZoomControl());
        display.addControlListener(new WheelZoomControl());
        display.addControlListener(new ZoomToFitControl());
        display.addControlListener(new NeighborHighlightControl());
        display.addControlListener(new MyControlAdapter("repaint"));
        
        // --------------------------------------------------------------------        
        // STEP 5: launching the visualization
        
        // create a panel for editing force values
        //final JForcePanel fpanel = new JForcePanel(fsim);
        
        
      
        /*final JValueSlider slider = new JValueSlider("", 0, maxhops, 0);
        
        slider.addChangeListener(new ChangeListener() {
            public void stateChanged(ChangeEvent e) {
                filter.setDistance(slider.getValue().intValue());
                vis.run("draw");
            }
        });
        slider.setBackground(Color.WHITE);
        slider.setPreferredSize(new Dimension(300,30));
        slider.setMaximumSize(new Dimension(300,300));*/
        
        Box cf = new Box(BoxLayout.Y_AXIS);
        //cf.add(slider);
        cf.setBorder(BorderFactory.createTitledBorder("Simulation Results"));
        //fpanel.add(text_label);
        //fpanel.add(cf);

        final JPanel fpanel = new JPanel();
         
        // create a new JSplitPane to present the interface
        JSplitPane split = new JSplitPane();
        split.setLeftComponent(display);
        split.setRightComponent(fpanel);
        split.setOneTouchExpandable(true);
        split.setContinuousLayout(true);
        split.setDividerLocation(800);
        split.setDividerLocation(800);
        
        
        // position and fix the default focus node
        NodeItem focus = (NodeItem)vg.getNode(0);
        PrefuseLib.setX(focus, null, 400);
        PrefuseLib.setY(focus, null, 250);
        focusGroup.setTuple(focus);
 
        VisualItem fv = (VisualItem)(vg.getNode(0));
        String result = "Simulation Results";
        result += "\n\r" ;
        result += "Total flits passed on the links: ";
        result+= fv.getString("link_utilization");
        result += "\n\r" ;
        result+=  "Total credits passed on the links: ";
        result+= fv.getString("link_cr_utilization");
        result += "\n\r" ;
        result+=  "Utilized BW: ";
        result+= fv.getString("bytes");
        result+= "*1e-6 Mbps";
        result += "\n\r" ;
        result += "Max BW available in the network: ";
        result+= fv.getString("av_bw");
        result+= "*1e-6 Mbps";
        result += "\n\r" ;
        result += "Total Packets: ";
        result+= fv.getString("pkts");
        result += "\n\r" ;
        result += "Total flits: ";
        result+= fv.getString("flits");
        result += "\n\r" ;
        result += "Simulation Time: ";
        result+= fv.getString("sim_time");
 
        final JTextArea textArea = new JTextArea();
        textArea.setEditable(false);
        
        textArea.setSize(100, 100);        
        textArea.setText(result);
        fpanel.setBackground(Color.WHITE);
        
      
        fpanel.add(textArea);
 

        // now we run our action list and return
        return split;
    }
    
} // end of class GraphView
