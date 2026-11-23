/*
 * resource.h - Resource and command identifiers for the GUI
 */
#ifndef GUI_RESOURCE_H
#define GUI_RESOURCE_H

#define IDI_APP             100
#define IDR_ACCEL           101

#define IDD_TRAIN           200
#define IDD_DELAY           201

/* Train dialog */
#define IDC_T_NUMBER        1001
#define IDC_T_NAME          1002
#define IDC_T_FROM          1003
#define IDC_T_TO            1004
#define IDC_T_TYPE          1005
#define IDC_T_COACHES       1006
#define IDC_T_COACHES_SPIN  1007
#define IDC_T_ARR           1008
#define IDC_T_DEP           1009
#define IDC_T_PLATFORM      1010
#define IDC_T_HINT          1011

/* Delay dialog */
#define IDC_D_TRAIN         1101
#define IDC_D_ADD           1102
#define IDC_D_RECOVER       1103
#define IDC_D_MIN           1104
#define IDC_D_SPIN          1105
#define IDC_D_REASON        1106
#define IDC_D_PREVIEW       1107

/* Main window controls */
#define IDC_LV_DASH         2001
#define IDC_LV_EVENTS       2002
#define IDC_LV_TRAINS       2003
#define IDC_LV_DELAYS       2004
#define IDC_LV_LOG          2005
#define IDC_SEARCH          2010
#define IDC_CHK_DEPARTED    2011
#define IDC_CHK_AUTOPF      2012
#define IDC_CHK_AUTOSIG     2013
#define IDC_STATUS          2020

/* Commands (menus, buttons, accelerators) */
#define IDM_SAVE            40001
#define IDM_LOAD            40002
#define IDM_EXPORT          40003
#define IDM_RESET           40004
#define IDM_EXIT            40005

#define IDM_RUN             40010
#define IDM_STEP1           40011
#define IDM_STEP5           40012
#define IDM_STEP15          40013
#define IDM_SPEED1          40014
#define IDM_SPEED2          40015
#define IDM_SPEED5          40016
#define IDM_SPEED10         40017

#define IDM_AUTOSIG         40020
#define IDM_RELEASE_ALL     40021
#define IDM_EMERGENCY       40022

#define IDM_AUTOPF          40030
#define IDM_AUTOASSIGN_ALL  40031

#define IDM_ADD_TRAIN       40040
#define IDM_EDIT_TRAIN      40041
#define IDM_DELAY_TRAIN     40042
#define IDM_RECOVER_TRAIN   40043
#define IDM_CLEAR_DELAY     40044
#define IDM_ASSIGN_TRAIN    40045
#define IDM_CANCEL_TRAIN    40046
#define IDM_REMOVE_TRAIN    40047
#define IDM_DETAILS         40048

#define IDM_OPEN_DATA       40050
#define IDM_LEGEND          40060
#define IDM_ABOUT           40061

#define IDM_PAGE_DASH       40100
#define IDM_PAGE_TRAINS     40101
#define IDM_PAGE_PLATFORMS  40102
#define IDM_PAGE_SIGNALS    40103
#define IDM_PAGE_DELAYS     40104
#define IDM_PAGE_LOG        40105

#endif /* GUI_RESOURCE_H */
