//
// Created by lakinduakash on 13/04/19.
//
/*
Copyright (c) 2019, lakinduaksh
        All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
        this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
        IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
        FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
        CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 */



#include <gtk/gtk.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <regex.h>

#include "h_prop.h"
#include "ui.h"
#include "read_config.h"
#include "util.h"

#define BUFSIZE 512
#define AP_ENABLED "AP-ENABLED"

#define INSTALL_PATH_PREFIX "/usr/share/wihotspot"
#define ERROR_SSID_MSG "SSID must not empty"
#define ERROR_PASS_MSG "Password must contain 8 characters"
#define ERROR_CHANNEL_MSG "Invalid channel number"
#define ERROR_CHANNEL_MSG_2 "Channel must be 1-11"
#define ERROR_CHANNEL_MSG_5 "Channel must be 1-196"
#define ERROR_MAC_MSG "Invalid Mac address"

GtkBuilder *builder;
GObject *window;
GtkButton *button_create_hp;
GtkButton *button_stop_hp;

GtkEntry *entry_ssd;
GtkEntry *entry_pass;
GtkEntry *entry_mac;
GtkEntry *entry_channel;

GtkComboBox *combo_wifi;
GtkComboBox *combo_internet;

GtkRadioButton *rb_freq_auto;
GtkRadioButton *rb_freq_2;
GtkRadioButton *rb_freq_5;

GtkCheckButton *cb_hidden;
GtkCheckButton *cb_psk;
GtkCheckButton *cb_mac;
GtkCheckButton *cb_novirt;
GtkCheckButton *cb_channel;
GtkCheckButton *cb_open;

GtkProgressBar *progress_bar;

GtkLabel *label_status;
GtkLabel *label_input_error;

GtkCssProvider* provider;
GdkDisplay *display;
GdkScreen *screen;

GError *error = NULL;

GtkStyleContext *context_entry_mac;
GtkStyleContext *context_entry_pass;
GtkStyleContext *context_entry_ssid;
GtkStyleContext *context_entry_channel;
GtkStyleContext *context_label_input_error;


const char** iface_list;
const char** wifi_iface_list;
int iface_list_length;
int wifi_iface_list_length;
char* running_info[3];
guint id;
static ConfigValues cv;



static void *stopHp() {
    if(running_info[0]!=NULL){
        gtk_label_set_label(label_status,"Stopping ...");
        start_pb_pulse();
        lock_all_views(TRUE);
        startShell(build_kill_create_ap_command(running_info[0]));
        g_thread_new("init_running",init_running_info,NULL);
    }
    return 0;
}

static void on_create_hp_clicked(GtkWidget *widget, gpointer data) {


    init_config_val_input(&cv);


    if(validator(&cv)==FALSE){
        set_error_text("Check inputs");
        return;
    }


    startShell(build_wh_mkconfig_command(&cv));

    g_thread_new("shell_create_hp", run_create_hp_shell, (void*)build_wh_from_config());


}

static void on_stop_hp_clicked(GtkWidget *widget, gpointer data) {
    g_thread_new("shell2", stopHp, NULL);

}


static void loadStyles(){
    provider = gtk_css_provider_new();
    display = gdk_display_get_default();
    screen = gdk_display_get_default_screen (display);
    gtk_style_context_add_provider_for_screen (screen, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER);

    const char *style_file;

    const char* debug_style_file="glade/style.css";
    const char* prod_style_file="/usr/share/wihotspot/glade/style.css";

    FILE *file;

    if ((file = fopen(debug_style_file, "r"))){
        fclose(file);
        style_file = debug_style_file;
        gtk_css_provider_load_from_path(GTK_CSS_PROVIDER(provider),"glade/style.css",NULL);

    }
    else if ((file = fopen(prod_style_file, "r"))){
        fclose(file);
        style_file = prod_style_file;

    } else{
        return;
    }

    gtk_css_provider_load_from_path(GTK_CSS_PROVIDER(provider),style_file,NULL);
}

static void init_style_contexts(){
    context_entry_mac = gtk_widget_get_style_context((GtkWidget*)entry_mac);
    context_entry_ssid = gtk_widget_get_style_context((GtkWidget*)entry_ssd);
    context_entry_pass = gtk_widget_get_style_context((GtkWidget*)entry_pass);
    context_entry_channel = gtk_widget_get_style_context((GtkWidget*)entry_channel);
    context_label_input_error = gtk_widget_get_style_context((GtkWidget*)label_input_error);

}

static void set_error_text(char * text){
    gtk_label_set_label(label_input_error,text);
}

static void* entry_mac_warn(GtkWidget *widget, gpointer data){

    const char *mac = gtk_entry_get_text(GTK_ENTRY(widget));
    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cb_mac))==TRUE) {

        if (mac == NULL || isValidMacAddress(mac) != 1) {
            set_error_text(ERROR_MAC_MSG);
            gtk_style_context_add_class(context_entry_mac, "entry-error");

            return NULL;
        }
    }

    gtk_style_context_remove_class(context_entry_mac,"entry-error");
    set_error_text("");
    return NULL;
}

static void* entry_ssid_warn(GtkWidget *widget, gpointer data){

    const char *ssid = gtk_entry_get_text(GTK_ENTRY(widget));

    if(ssid !=NULL)
    {
        size_t len = strlen(ssid);

        if(len<1){
            gtk_style_context_add_class(context_entry_ssid, "entry-error");
            set_error_text(ERROR_SSID_MSG);
            return NULL;
        } else{
            gtk_style_context_remove_class(context_entry_ssid, "entry-error");
            set_error_text("");
        }
    }


    if(ssid ==NULL)
    {
        gtk_style_context_add_class(context_entry_ssid, "entry-error");
        set_error_text(ERROR_SSID_MSG);
        return FALSE;
    }

    gtk_style_context_remove_class(context_entry_ssid,"entry-error");
    set_error_text("");
    return NULL;
}


static void* entry_pass_warn(GtkWidget *widget, gpointer data){

    const char *pass = gtk_entry_get_text(GTK_ENTRY(widget));

    if(pass !=NULL)
    {
        size_t len = strlen(pass);

        if(len<8 && len>0){
            gtk_style_context_add_class(context_entry_pass, "entry-error");
            set_error_text(ERROR_PASS_MSG);
            return NULL;
        } else{
            gtk_style_context_remove_class(context_entry_pass, "entry-error");
        }
    }

    gtk_style_context_remove_class(context_entry_mac,"entry-error");
    set_error_text("");
    return NULL;
}

static void* entry_channel_warn(GtkWidget *widget, gpointer data){

    const char *channel = gtk_entry_get_text(GTK_ENTRY(widget));

    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cb_channel))==TRUE) {

        if (channel == NULL) {
            gtk_style_context_add_class(context_entry_channel, "entry-error");
            set_error_text(ERROR_CHANNEL_MSG);
            return FALSE;
        }


        char *end;
        long li;
        char *c = strdup(channel);
        li = strtol(c, &end, 10);

        if (end == c) {
            gtk_style_context_add_class(context_entry_channel, "entry-error");
            set_error_text(ERROR_CHANNEL_MSG);
            return FALSE;
        } else if ('\0' != *end) {
            gtk_style_context_add_class(context_entry_channel, "entry-error");
            set_error_text(ERROR_CHANNEL_MSG);
            return FALSE;
        }


        if (cv.freq == NULL) {
            if (!(li <= 196 && li > 0)) {
                gtk_style_context_add_class(context_entry_channel, "entry-error");
                set_error_text(ERROR_CHANNEL_MSG);
                return FALSE;
            }
        } else if (strcmp(cv.freq, "2.4") == 0) {
            if (!(li <= 11 && li > 0)) {
                gtk_style_context_add_class(context_entry_channel, "entry-error");
                set_error_text(ERROR_CHANNEL_MSG_2);
                return FALSE;
            }
        } else if (strcmp(cv.freq, "5") == 0) {
            if (!(li <= 196 && li > 0)) {
                gtk_style_context_add_class(context_entry_channel, "entry-error");
                set_error_text(ERROR_CHANNEL_MSG_5);
                return FALSE;
            }
        }

    }
    gtk_style_context_remove_class(context_entry_channel,"entry-error");
    set_error_text("");
    return NULL;
}


static void *update_freq_toggle(){
    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rb_freq_2)))
        cv.freq = "2.4";

    else if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rb_freq_5)))
        cv.freq ="5";
    else
        cv.freq =NULL;

    return NULL;
}


int initUi(int argc, char *argv[]){

    XInitThreads();

    gtk_init(&argc, &argv);

    /* Construct a GtkBuilder instance and load our UI description */

//
//    DIR* dir = opendir("mydir");
//    if (dir)
//    {
//        /* Directory exists. */
//        closedir(dir);
//    }
//    else if (ENOENT == errno)
//    {
//        /* Directory does not exist. */
//    }
//    else
//    {
//        /* opendir() failed for some other reason. */
//    }

    const char* debug_glade_file="glade/wifih.ui";
    const char* prod_glade_file="/usr/share/wihotspot/glade/wifih.ui";

    FILE *file;
    builder = gtk_builder_new();

    if ((file = fopen(debug_glade_file, "r"))){
        fclose(file);
        if (gtk_builder_add_from_file(builder, debug_glade_file, &error) == 0) {
            g_printerr("Error loading file: %s\n", error->message);
            g_clear_error(&error);
            return 1;
        }
    }
    else if ((file = fopen(prod_glade_file, "r"))){
        fclose(file);
        if (gtk_builder_add_from_file(builder, prod_glade_file, &error) == 0) {
            g_printerr("Error loading file: %s\n", error->message);
            g_clear_error(&error);
            return 1;
        }
    } else{
        return 1;
    }


    /* Connect signal handlers to the constructed widgets. */
    window = gtk_builder_get_object(builder, "window");
    g_signal_connect (window, "destroy", G_CALLBACK(gtk_main_quit), NULL);


    button_create_hp = (GtkButton *) gtk_builder_get_object(builder, "button_create_hp");
    button_stop_hp = (GtkButton *) gtk_builder_get_object(builder, "button_stop_hp");

    entry_ssd = (GtkEntry *) gtk_builder_get_object(builder, "entry_ssid");
    entry_pass = (GtkEntry *) gtk_builder_get_object(builder, "entry_pass");

    entry_mac = (GtkEntry *) gtk_builder_get_object(builder, "entry_mac");
    entry_channel = (GtkEntry *) gtk_builder_get_object(builder, "entry_channel");

    combo_wifi = (GtkComboBox *) gtk_builder_get_object(builder, "combo_wifi");
    combo_internet = (GtkComboBox *) gtk_builder_get_object(builder, "combo_internet");

    cb_hidden = (GtkCheckButton *) gtk_builder_get_object(builder, "cb_hidden");
    cb_psk = (GtkCheckButton *) gtk_builder_get_object(builder, "cb_psk");
    cb_mac = (GtkCheckButton *) gtk_builder_get_object(builder, "cb_mac");
    cb_novirt = (GtkCheckButton *) gtk_builder_get_object(builder, "cb_novirt");
    cb_channel = (GtkCheckButton *) gtk_builder_get_object(builder, "cb_channel");
    cb_open = (GtkCheckButton *) gtk_builder_get_object(builder, "cb_open");

    rb_freq_auto = (GtkRadioButton *) gtk_builder_get_object(builder, "rb_freq_auto");
    rb_freq_2 = (GtkRadioButton *) gtk_builder_get_object(builder, "rb_freq_2");
    rb_freq_5 = (GtkRadioButton *) gtk_builder_get_object(builder, "rb_freq_5");

    label_status = (GtkLabel *) gtk_builder_get_object(builder, "label_status");
    label_input_error = (GtkLabel *) gtk_builder_get_object(builder, "label_input_error");

    progress_bar = (GtkProgressBar *) gtk_builder_get_object(builder, "progress_bar");

    loadStyles();
    init_style_contexts();

    gtk_style_context_add_class(context_label_input_error, "label-error");


    //gtk_entry_set_visibility(entry_pass,FALSE);

    g_signal_connect (button_create_hp, "clicked", G_CALLBACK(on_create_hp_clicked), NULL);
    g_signal_connect (button_stop_hp, "clicked", G_CALLBACK(on_stop_hp_clicked), NULL);

    g_signal_connect (entry_mac, "changed", G_CALLBACK(entry_mac_warn), NULL);
    g_signal_connect (entry_ssd, "changed", G_CALLBACK(entry_ssid_warn), NULL);
    g_signal_connect (entry_pass, "changed", G_CALLBACK(entry_pass_warn), NULL);

    g_signal_connect (entry_channel, "changed", G_CALLBACK(entry_channel_warn), NULL);

    g_signal_connect (rb_freq_2, "toggled", G_CALLBACK(update_freq_toggle), NULL);
    g_signal_connect (rb_freq_5, "toggled", G_CALLBACK(update_freq_toggle), NULL);
    g_signal_connect (rb_freq_auto, "toggled", G_CALLBACK(update_freq_toggle), NULL);


    start_pb_pulse();
    g_thread_new("init_running",init_running_info,NULL);

    init_interface_list();
    init_ui_from_config();


    gtk_main();

    return 0;
}


void init_ui_from_config(){

    if(read_config_file()==READ_CONFIG_FILE_SUCCESS){

        ConfigValues *values=getConfigValues();


        if(values->ssid!=NULL)
            gtk_entry_set_text(entry_ssd,values->ssid);
        if(values->pass!=NULL)
            gtk_entry_set_text(entry_pass,values->pass);

        if(values->iface_wifi!=NULL){
            int idw=find_str(values->iface_wifi,wifi_iface_list,wifi_iface_list_length);

            if(idw !=-1){
                gtk_combo_box_set_active(combo_wifi,idw);
            }
        }

        if(values->iface_inet!=NULL){
            int idw=find_str(values->iface_inet,iface_list,iface_list_length);

            if(idw !=-1){
                gtk_combo_box_set_active(combo_internet,idw);
            }
        }

    }
}

void init_interface_list(){

    iface_list_length=0;
    wifi_iface_list_length=0;
    iface_list=(const char**)get_interface_list(&iface_list_length);
    wifi_iface_list=(const char**)get_wifi_interface_list(&wifi_iface_list_length);

    for (int i = 0; i < iface_list_length; i++){

        gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo_internet), iface_list[i]);
    }

    for (int i = 0; i < wifi_iface_list_length; i++){
        gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo_wifi), wifi_iface_list[i]);
    }

}


void lock_all_views(gboolean set_lock){
    if(set_lock){
        gtk_editable_set_editable( (GtkEditable*)entry_ssd,FALSE);
        gtk_editable_set_editable( (GtkEditable*)entry_pass,FALSE);
        gtk_widget_set_sensitive ((GtkWidget*)button_create_hp, FALSE);
        gtk_widget_set_sensitive ((GtkWidget*)button_stop_hp, FALSE);
        gtk_widget_set_sensitive ((GtkWidget*)combo_internet, FALSE);
        gtk_widget_set_sensitive ((GtkWidget*)combo_wifi, FALSE);
    } else{
        gtk_editable_set_editable( (GtkEditable*)entry_ssd,TRUE);
        gtk_editable_set_editable( (GtkEditable*)entry_pass,TRUE);
        gtk_widget_set_sensitive ((GtkWidget*)button_create_hp, TRUE);
        gtk_widget_set_sensitive ((GtkWidget*)button_stop_hp, TRUE);
        gtk_widget_set_sensitive ((GtkWidget*)button_stop_hp, TRUE);
        gtk_widget_set_sensitive ((GtkWidget*)combo_internet, TRUE);
        gtk_widget_set_sensitive ((GtkWidget*)combo_wifi, TRUE);
    }
}


void lock_running_views(gboolean set_lock){
    if(set_lock){
        gtk_editable_set_editable( (GtkEditable*)entry_ssd,FALSE);
        gtk_editable_set_editable( (GtkEditable*)entry_pass,FALSE);
        gtk_widget_set_sensitive ((GtkWidget*)button_create_hp, FALSE);

        gtk_widget_set_sensitive ((GtkWidget*)button_stop_hp, TRUE);

        gtk_widget_set_sensitive ((GtkWidget*)combo_internet, FALSE);
        gtk_widget_set_sensitive ((GtkWidget*)combo_wifi, FALSE);
    } else{
        gtk_editable_set_editable( (GtkEditable*)entry_ssd,TRUE);
        gtk_editable_set_editable( (GtkEditable*)entry_pass,TRUE);
        gtk_widget_set_sensitive ((GtkWidget*)button_create_hp, TRUE);

        gtk_widget_set_sensitive ((GtkWidget*)button_stop_hp, FALSE);

        gtk_widget_set_sensitive ((GtkWidget*)combo_internet, TRUE);
        gtk_widget_set_sensitive ((GtkWidget*)combo_wifi, TRUE);
    }
}

static guint start_pb_pulse(){
    gtk_widget_set_visible((GtkWidget*)progress_bar,TRUE);
    return id= g_timeout_add (100, update_progress_in_timeout, progress_bar);
}

static void stop_pb_pulse(){
    g_source_remove(id);
    gtk_widget_set_visible((GtkWidget*)progress_bar,FALSE);
}

static gboolean update_progress_in_timeout (gpointer pbar)
{
    gtk_progress_bar_pulse (pbar);
    return TRUE; /* keep running */
}


void clear_running_info(){

        g_free(running_info[0]);
        running_info[0]=NULL;
}

void* init_running_info(){

    clear_running_info();
    lock_all_views(TRUE);

    gtk_label_set_label(label_status,"Getting running info...");

    get_h_running_info(running_info);

    if(running_info[0]!=NULL){

        char a[BUFSIZE];
        snprintf(a,BUFSIZE,"Running as PID: %s",running_info[0]);
        gtk_label_set_label(label_status,a);

        lock_all_views(FALSE);
        lock_running_views(TRUE);


    } else{

        gtk_label_set_label(label_status,"Not running");
        lock_all_views(FALSE);
        lock_running_views(FALSE);
    }

    stop_pb_pulse();
    return 0;
}


static void *run_create_hp_shell(void *cmd) {

    char buf[BUFSIZE];
    char buf2[BUFSIZE];
    FILE *fp;

    if(cv.freq){
        cmd = strcat( cmd, " --freq-band ");
        cmd = strcat( cmd, cv.freq);

        if ((fp = popen(cmd, "r")) == NULL) {
            printf("Error opening pipe!\n");
            return NULL;
        }
    }
    else{
        if ((fp = popen(cmd, "r")) == NULL) {
            printf("Error opening pipe!\n");
            return NULL;
        }
    }



    start_pb_pulse();

    while (fgets(buf, BUFSIZE, fp) != NULL) {
        buf[strcspn(buf, "\n")] = 0;
        gtk_label_set_label(label_status,buf);

        if (strstr(buf, AP_ENABLED) != NULL) {
            init_running_info();
            return 0;
        }
    }

    if (pclose(fp)) {
        printf("Command not found or exited with error status\n");
        init_running_info();
        return NULL;
    }

    init_running_info();
    return 0;
}


static gboolean validator(ConfigValues *cv){


    if(cv->pass !=NULL)
    {
        size_t len = strlen(cv->pass);

        if(len<8){
            if(len>0)
            return FALSE;
        }
    }

    if(cv->ssid !=NULL)
    {
        size_t len = strlen(cv->ssid);

        if(len<1){
            return FALSE;
        }
    }

    if(cv->ssid ==NULL)
    {
        return FALSE;
    }

    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cb_mac))==TRUE) {

        if (cv->mac == NULL || isValidMacAddress(cv->mac) != 1) {
            set_error_text(ERROR_MAC_MSG);
            gtk_style_context_add_class(context_entry_mac, "entry-error");

            return FALSE;
        }
    }

    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cb_channel))==TRUE){

        if(cv->channel==NULL)
            return FALSE;

        char *end;
        long li;
        char *c=strdup(cv->channel);
        li = strtol(c,&end,10);

        if (end == c) {
            fprintf(stderr, "%s: not a decimal number\n", c);
            return FALSE;
        } else if ('\0' != *end) {
            fprintf(stderr, "%s: extra characters at end of input: %s\n", c, end);
            return FALSE;
        }


        if(cv->freq==NULL){
            if(!(li<=196 && li>0))
                return FALSE;
        }
        else if(strcmp(cv->freq,"2.4")==0){
            if(!(li<=11 && li>0))
                return FALSE;
        } else if(strcmp(cv->freq,"5")==0){
            if(!(li<=196 && li>0))
                return FALSE;
        }

    }


    return TRUE;
}

static int init_config_val_input(ConfigValues* cv){


    if (gtk_combo_box_get_active (combo_wifi) >= 0 && gtk_combo_box_get_active (combo_internet) >= 0) {


        gchar *wifi =cv->iface_wifi= gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT(combo_wifi));
        gchar *internet =cv->iface_inet= gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT(combo_internet));

        //Remove trailing new lines
        wifi[strcspn(wifi, "\n")] = 0;
        internet[strcspn(internet, "\n")] = 0;


        cv->ssid = (char *) gtk_entry_get_text(entry_ssd);

        if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cb_open))){
            cv->pass = "";
        }
        else{
            cv->pass = (char *) gtk_entry_get_text(entry_pass);
        }

        if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rb_freq_2)))
            cv->freq = "2.4";

        else if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rb_freq_5)))
            cv->freq ="5";
        else
            cv->freq =NULL;

        if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cb_channel)))
            cv->channel = (char*)gtk_entry_get_text(entry_channel);
        else
            cv->channel = NULL;


        if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cb_mac)))
            cv->mac = (char*)gtk_entry_get_text(entry_mac);
        else
            cv->mac =NULL;

        if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cb_hidden)))
            cv->hidden = "1";

        else
            cv->hidden =NULL;


        if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cb_novirt)))
            cv->no_virt = "1";
        else
            cv->no_virt=NULL;

        if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cb_psk)))
            cv->use_psk = "1";
        else
            cv->use_psk =NULL;

        return 0;

    } else{

        g_print("Please select Wifi and Internet interfaces\n");
        return 1;
    }

}
