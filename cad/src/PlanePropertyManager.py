# Copyright 2007 Nanorex, Inc.  See LICENSE file for details. 
"""

@author: Ninad,
@copyright: 2007 Nanorex, Inc.  See LICENSE file for details.
@version:$Id$

History:
ninad 20070602: Created.

"""
__author__ = "Ninad"

from PyQt4.Qt import SIGNAL
from PyQt4.Qt import Qt
from PyQt4.Qt import QAction
from PyQt4.Qt import QActionGroup
from PyQt4.Qt import QButtonGroup

from PM.PM_Dialog        import PM_Dialog
from PM.PM_GroupBox      import PM_GroupBox
from PM.PM_DoubleSpinBox import PM_DoubleSpinBox
from PM.PM_ComboBox      import PM_ComboBox
from PM.PM_SpinBox       import PM_SpinBox
from PM.PM_PushButton    import PM_PushButton
from PM.PM_CheckBox      import PM_CheckBox
from PM.PM_RadioButton   import PM_RadioButton
from PM.PM_RadioButtonList import PM_RadioButtonList

from PM.PM_Constants     import pmRestoreDefaultsButton

# Placement Options radio button list to create radio button list.
# Format: buttonId, buttonText, tooltip
PLACEMENT_OPTIONS_BUTTON_LIST = [ \
    ( 0, "Parallel to screen",     "Parallel to screen"     ),
    ( 1, "Through selected atoms", "Through selected atoms" ),
    ( 2, "Offset to a plane",      "Offset to a plane"      ),
    ( 3, "Custom",                 "Custom"                 )
]
    
class PlanePropertyManager(PM_Dialog):
    """
    The PlanePropertyManager class provides a Property Manager for a 
    (reference) Plane.
    """
    
    # The title that appears in the Property Manager header.
    title = "Plane"
    # The name of this Property Manager. This will be set to
    # the name of the PM_Dialog object via setObjectName().
    pmName = title
    # The relative path to the PNG file that appears in the header
    iconPath = "ui/actions/Insert/Reference Geometry/Plane.png"
    
    def __init__(self, plane):
        """
        Construct the Plane Property Manager.
        
        @param plane: The plane.
        @type  plane: L{Plane}
        """
        
        self.geometry = plane
        PM_Dialog.__init__( self, self.pmName, self.iconPath, self.title )       
        self._addGroupBoxes()
        self._addWhatsThisText()
        
        msg = "Insert a Plane parallel to the screen. Note: This feature is \
        experimental for Alpha9 and has known bugs."
        
        # This causes the "Message" box to be displayed as well.
        self.MessageGroupBox.insertHtmlMessage(msg, setAsDefault=False)
        
        # self.resized_from_glpane flag makes sure that the spinbox.valueChanged()
        # signal is not emitted after calling spinbox.setValue.
        self.resized_from_glpane = False
        
        # Hide Preview and Restore defaults button for Alpha9.
        self.hideTopRowButtons(pmRestoreDefaultsButton)
      
    def _addGroupBoxes(self):
        """
        Add the 1st group box to the Property Manager.
        """
        self.pmGroupBox1 = PM_GroupBox(self, title = "Parameters")
        self._loadGroupBox1(self.pmGroupBox1)
        
        self.pmPlacementOptions = \
            PM_RadioButtonList( self,
                                title      = "Placement Options", 
                                buttonList = PLACEMENT_OPTIONS_BUTTON_LIST,
                                checkedId  = 3 )
        
        self.connect(self.pmPlacementOptions.buttonGroup,
                     SIGNAL("buttonClicked(int)"),
                     self.geometry.changePlanePlacement)
              
    def _loadGroupBox1(self, pmGroupBox):
        """
        Load widgets in 1st group box.
        
        @param pmGroupBox: The 1st group box in the PM.
        @type  pmGroupBox: L{PM_GroupBox}
        """
        
        self.widthDblSpinBox = \
            PM_DoubleSpinBox(pmGroupBox,
                             label        = "Width:",
                             value        = 10.0, 
                             setAsDefault = True,
                             minimum      = 1.0, 
                             maximum      = 200.0,
                             singleStep   = 1.0, 
                             decimals     = 1, 
                             suffix       = ' Angstroms')
        
        self.connect(self.widthDblSpinBox, 
                     SIGNAL("valueChanged(double)"), 
                     self.change_plane_width)
                
        self.heightDblSpinBox = \
            PM_DoubleSpinBox(pmGroupBox, 
                             label        =" Height:",
                             value        = 10.0, 
                             setAsDefault = True,
                             minimum      = 1.0, 
                             maximum      = 200.0,
                             singleStep   = 1.0, 
                             decimals     = 1, 
                             suffix       = ' Angstroms')
        
        self.connect(self.heightDblSpinBox, 
                     SIGNAL("valueChanged(double)"), 
                     self.change_plane_height)
            
        self.aspectRatioCheckBox = \
            PM_CheckBox(pmGroupBox,
                        text         = 'Maintain Aspect Ratio of:' ,
                        widgetColumn = 1,
                        state        = Qt.Unchecked
                        )
        
        self.connect(self.aspectRatioCheckBox,
                     SIGNAL("stateChanged(int)"),
                     self._enableAspectRatioSpinBox)
        
        self.aspectRatioSpinBox = \
            PM_DoubleSpinBox( pmGroupBox,
                              label         =  "",
                              value         =  2.0,
                              setAsDefault  =  True,
                              minimum       =  0.1,
                              maximum       =  10.0,
                              singleStep    =  0.1,
                              decimals      =  2,
                              suffix        =  " to 1.00")   
        
        if self.aspectRatioCheckBox.isChecked():
            self.aspectRatioSpinBox.setEnabled(True)
        else:
            self.aspectRatioSpinBox.setEnabled(False)
                
    def _addWhatsThisText(self):
        """
        Add "What's This" text for all widgets in this Property Manager.
        """    
        self.heightDblSpinBox.setWhatsThis("""<b>Height</b>
        <p>The height of the Plane in angstroms.
        (up to 200 Angstroms)</p>""")
        
        self.widthDblSpinBox.setWhatsThis("""<b>Width</b>
        <p>The width of the Plane in angstroms.
        (up to 200 Angstroms)</p>""")
        pass
        
    def show(self):
        """
        Show the Plane Property Manager.
        """
        self.update_spinboxes()
        PM_Dialog.show(self)   
        self.geometry.updateCosmeticProps(previewing = True)
                
    def change_plane_width(self):
        """
        Slot for width spinbox in the Property Manager.
        """
        if self.aspectRatioCheckBox.isChecked():
            self.geometry.width   =  self.widthDblSpinBox.value()
            self.geometry.height  =  self.geometry.width / \
                                     self.aspectRatioSpinBox.value() 
            self.update_spinboxes()
        else:
            self.change_plane_size()
        self._updateAspectRatio()
    
    def change_plane_height(self):
        """
        Slot for height spinbox in the Property Manager.
        """
        if self.aspectRatioCheckBox.isChecked():
            self.geometry.height  =  self.heightDblSpinBox.value() 
            self.geometry.width   =  self.geometry.height * \
                                     self.aspectRatioSpinBox.value()
            self.update_spinboxes()
        else:
            self.change_plane_size()
        self._updateAspectRatio()
        
    def change_plane_size(self, gl_update = True):
        """
        Slot to change the Plane's width and height.
        
        @param gl_update: Forces an update of the glpane.
        @type  gl_update: bool
        """
        if not self.resized_from_glpane:
            self.geometry.width   =  self.widthDblSpinBox.value()
            self.geometry.height  =  self.heightDblSpinBox.value() 
        if gl_update:
            self.geometry.glpane.gl_update()
    
    def update_spinboxes(self):
        """
        Update the width and height spinboxes.
        """
        # self.resized_from_glpane flag makes sure that the spinbox.valueChanged()
        # signal is not emitted after calling spinbox.setValue(). 
        # This flag is used in change_plane_size method.-- Ninad 20070601
        self.resized_from_glpane = True
        self.heightDblSpinBox.setValue(self.geometry.height)
        self.widthDblSpinBox.setValue(self.geometry.width)
        self.geometry.glpane.gl_update()
        self.resized_from_glpane = False
    
    def _enableAspectRatioSpinBox(self, enable):
        """
        Slot for "Maintain Aspect Ratio" checkbox which enables or disables
        the Aspect Ratio spin box.
        
        @param enable: True = enable, False = disable.
        @type  enable: bool
        """
        
        self.aspectRatioSpinBox.setEnabled(enable)

    def _updateAspectRatio(self):
        """
        Updates the Aspect Ratio spin box based on the current width and height.
        """
        aspectRatio = self.geometry.width / self.geometry.height
        self.aspectRatioSpinBox.setValue(aspectRatio)