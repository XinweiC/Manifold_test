package prefuse.demos.applets;

import java.awt.Color;
import java.awt.Shape;
import java.awt.geom.AffineTransform;
import java.awt.geom.Point2D;
import java.util.Iterator;

import prefuse.Constants;
import prefuse.data.Edge;
import prefuse.data.Node;
import prefuse.render.EdgeRenderer;
import prefuse.util.ColorLib;
import prefuse.util.GraphicsLib;
import prefuse.visual.EdgeItem;
import prefuse.visual.VisualItem;

public class MyMultiEdgeRenderer extends EdgeRenderer {
	private boolean useStraightLineForSingleEdges;

	public MyMultiEdgeRenderer(int edgeTypeCurve, int edgeArrowForward) {
		super(edgeTypeCurve, edgeArrowForward);
	}

	@Override
	protected void getCurveControlPoints(EdgeItem eitem, Point2D[] cp, double x1, double y1, double x2, double y2) {
		// how many from '1' to '2'?
		Node sourceNode = eitem.getSourceNode();
		Node targetNode = eitem.getTargetNode();
		Iterator edges = sourceNode.edges();
		// number of equal edges = same target and source
		int noOfEqualEdges = 0;
		// number of nearequal edges = same nodes, but any order target and
		// source
		int noOfSameNodeEdges = 0;
		int myEdgeIndex = 0;
		int row = eitem.getRow();
		while (edges.hasNext()) {
			Edge edge = (Edge) edges.next();
			int edgeRow = edge.getRow();
			if (edge.getSourceNode() == sourceNode && edge.getTargetNode() == targetNode) {
				if (row == edgeRow) {
					myEdgeIndex = noOfEqualEdges;
				}
				noOfEqualEdges++;
				noOfSameNodeEdges++;
			} else if (edge.getSourceNode() == targetNode && edge.getTargetNode() == sourceNode) {
				noOfSameNodeEdges++;
			}
		}
		double dx = x2 - x1, dy = y2 - y1;

		// modify to add an offset relative to what this edge's index is
		dx = dx * (1 + myEdgeIndex);
		dy = dy * (1 + myEdgeIndex);
		cp[0].setLocation(x1 + 3 * dx / 4, y1);
		cp[1].setLocation(x2 - dx / 10, y2 - dy / 10);

		if (useStraightLineForSingleEdges && myEdgeIndex == 0 && noOfSameNodeEdges == 1) {
			cp[0].setLocation(x2, y2);
			cp[1].setLocation(x1, y1);
		}
	}

	public boolean isUseStraightLineForSingleEdges() {
		return useStraightLineForSingleEdges;
	}

	public void setUseStraightLineForSingleEdges(boolean useStraightLineForSingleEdges) {
		this.useStraightLineForSingleEdges = useStraightLineForSingleEdges;
	}
	
    /**
     * @see prefuse.render.AbstractShapeRenderer#getRawShape(prefuse.visual.VisualItem)
     */
	@Override
    protected Shape getRawShape(VisualItem item) {
        EdgeItem   edge = (EdgeItem)item;
        VisualItem item1 = edge.getSourceItem();
        VisualItem item2 = edge.getTargetItem();
        
        //int type = m_edgeType;
        int type = (Integer)edge.get("type");
        
        if(type == 1) {
        	m_edgeType = 1;
        	type = 1;
        	edge.setSize(2);
        	//edge.setStrokeColor(ColorLib.blue(0));
        }
        else {
        	m_edgeType = 0;
        	type = 0;
        	edge.setSize(4);
        	//edge.setStrokeColor(ColorLib.green(0));
        }
        
        getAlignedPoint(m_tmpPoints[0], item1.getBounds(),
                        m_xAlign1, m_yAlign1);
        getAlignedPoint(m_tmpPoints[1], item2.getBounds(),
                        m_xAlign2, m_yAlign2);
        m_curWidth = (float)(m_width * getLineWidth(item));
        
        // create the arrow head, if needed
        EdgeItem e = (EdgeItem)item;
        if ( e.isDirected() && m_edgeArrow != Constants.EDGE_ARROW_NONE ) {
            // get starting and ending edge endpoints
            boolean forward = (m_edgeArrow == Constants.EDGE_ARROW_FORWARD);
            Point2D start = null, end = null;
            start = m_tmpPoints[forward?0:1];
            end   = m_tmpPoints[forward?1:0];
            
            // compute the intersection with the target bounding box
            VisualItem dest = forward ? e.getTargetItem() : e.getSourceItem();
            int i = GraphicsLib.intersectLineRectangle(start, end,
                    dest.getBounds(), m_isctPoints);
            if ( i > 0 ) end = m_isctPoints[0];
            
            // create the arrow head shape
            AffineTransform at = getArrowTrans(start, end, m_curWidth);
            m_curArrow = at.createTransformedShape(m_arrowHead);
            
            // update the endpoints for the edge shape
            // need to bias this by arrow head size
            Point2D lineEnd = m_tmpPoints[forward?1:0]; 
            lineEnd.setLocation(0, -m_arrowHeight);
            at.transform(lineEnd, lineEnd);
        } else {
            m_curArrow = null;
        }
        
        // create the edge shape
        Shape shape = null;
        double n1x = m_tmpPoints[0].getX();
        double n1y = m_tmpPoints[0].getY();
        double n2x = m_tmpPoints[1].getX();
        double n2y = m_tmpPoints[1].getY();
        switch ( type ) {
            case Constants.EDGE_TYPE_LINE:          
                m_line.setLine(n1x, n1y, n2x, n2y);
                shape = m_line;
                break;
            case Constants.EDGE_TYPE_CURVE:
                getCurveControlPoints(edge, m_ctrlPoints,n1x,n1y,n2x,n2y);
                m_cubic.setCurve(n1x, n1y,
                                m_ctrlPoints[0].getX(), m_ctrlPoints[0].getY(),
                                m_ctrlPoints[1].getX(), m_ctrlPoints[1].getY(),
                                n2x, n2y);
                shape = m_cubic;
                break;
            default:
                throw new IllegalStateException("Unknown edge type");
        }
        
        // return the edge shape
        return shape;
    }
} 
