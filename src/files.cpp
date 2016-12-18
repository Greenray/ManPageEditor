/*
 * ©K. D. Hedger. Fri 27 Nov 11:42:28 GMT 2015 kdhedger68713@gmail.com
 *  Victor Nabatov Sun 12 Dec 10:32:00 GMT 2016 greenray.spb@gmail.com
 *
 * This file (callbacks.cpp) is part of MPE-gtk2.
 *
 * MPE-gtk2 is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or at your
 * option any later version.
 *
 * MPE-gtk2 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with MPE-gtk2. If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib.h>
#include <gtksourceview/gtksourceview.h>

#include "callbacks.h"
#include "script.h"

char  tFileName[]  = "/tmp/mpe-gtk2XXXXXX";
/* Working file */
char* saveFileName = NULL;
/* Working path */
char* saveFilePath = NULL;
bool  dropTextFile = false;
bool  isSubsection = false;

pageStruct* importPage = NULL;

int tagboldnum      = 0;
int tagitalicnum    = 0;
int selectedSection = 1;

void tagLookUp(GtkTextTag* tag, gpointer data) {
	int style     = -1;
	int underline = -1;

	if (useUnderline == true) {
		g_object_get((gpointer)tag, "style", &style, NULL);
		if (style == PANGO_STYLE_ITALIC) {
            g_object_set((gpointer)tag, "style",     PANGO_STYLE_NORMAL,     NULL);
			g_object_set((gpointer)tag, "underline", PANGO_UNDERLINE_SINGLE, NULL);
		}
	} else {
		g_object_get((gpointer)tag, "underline", &underline, NULL);
		if (underline == PANGO_UNDERLINE_SINGLE) {
            g_object_set((gpointer)tag, "underline", PANGO_UNDERLINE_NONE, NULL);
			g_object_set((gpointer)tag, "style",     PANGO_STYLE_ITALIC,   NULL);
		}
    }
}

void setUnderlining(pageStruct* page) {
	GtkTextTagTable* tagtable = gtk_text_buffer_get_tag_table((GtkTextBuffer*)page->buffer);
	gtk_text_tag_table_foreach(tagtable, tagLookUp, page->fileName);
}

void makeDirty(GtkWidget* widget, gpointer data) {
	dirty = true;
	setSensitive();
}

void resetAllItalicTags(void) {
	pageStruct* page;

	int numtabs = gtk_notebook_get_n_pages(notebook);

	for (int loop = 0; loop < numtabs; loop++) {
        page = getPageStructPtr(loop);
		if (page != NULL) {
            setUnderlining(page);
			dirty = true;
            gtk_text_buffer_set_modified(GTK_TEXT_BUFFER(page->buffer), true);
            setSensitive();
        }
    }
}

GtkWidget* makeNewTab(char* name, char* tooltip, pageStruct* page) {
	GtkWidget* evbox = gtk_event_box_new();
	GtkWidget* hbox  = NULL;

	hbox = creatNewBox(NEWHBOX, false, 0);

	GtkWidget* label = gtk_label_new(name);

	gtk_box_pack_start(GTK_BOX(hbox), label, false, false, 0);
	gtk_container_add(GTK_CONTAINER(evbox), hbox);
	g_signal_connect(G_OBJECT(evbox), "button-press-event", G_CALLBACK(tabPopUp), (void*)page);

	page->tabName = label;
	gtk_widget_show_all(evbox);

	return(evbox);
}

void setFilePrefs(GtkSourceView* sourceview) {
	PangoFontDescription* font_desc;

	gtk_source_view_set_auto_indent(sourceview, true);
	gtk_source_view_set_show_line_numbers(sourceview, false);
	gtk_source_view_set_highlight_current_line(sourceview, highLight);

	if (lineWrap == true)
		 gtk_text_view_set_wrap_mode((GtkTextView*)sourceview, GTK_WRAP_WORD_CHAR);
	else gtk_text_view_set_wrap_mode((GtkTextView*)sourceview, GTK_WRAP_NONE);

	gtk_source_view_set_tab_width(sourceview, tabWidth);

	font_desc = pango_font_description_from_string(fontAndSize);
	gtk_widget_modify_font((GtkWidget*)sourceview, font_desc);
	pango_font_description_free(font_desc);
}

void resetAllFilePrefs(void) {
	pageStruct* page;

	for (int loop = 0; loop < gtk_notebook_get_n_pages(notebook); loop++) {
        page = getPageStructPtr(loop);
		setFilePrefs(page->view);
    }
}

void dropText(GtkWidget* widget,
              GdkDragContext* context,
              gint x,
              gint y,
              GtkSelectionData* selection_data,
//              guint    info,
              guint32  time,
              gpointer user_data) {
	gchar**  array = NULL;
	int      cnt;
	char*    filename;
	FILE*    fp;
	char*    command;
	GString* str;
	char     line[1024];

	pageStruct*  page = (pageStruct*)user_data;
	GtkTextIter  iter;
	GtkTextMark* mark;

	array = gtk_selection_data_get_uris(selection_data);
	if (array == NULL) {
        gtk_drag_finish (context, true, true, time);
		return;
    }

	cnt = g_strv_length(array);

	if (dropTextFile == false) {
		dropTextFile = true;

		for (int j = 0; j < cnt; j++) {
            str = g_string_new(NULL);
			filename = g_filename_from_uri(array[j], NULL, NULL);
			asprintf(&command, "cat %s", filename);
			fp = popen(command, "r");
			while (fgets(line, 1024, fp)) {
				g_string_append_printf(str, "%s", line);
            }
			gtk_text_buffer_insert_at_cursor(GTK_TEXT_BUFFER(page->buffer), str->str, str->len);

			mark = gtk_text_buffer_get_insert((GtkTextBuffer*)page->buffer);
			gtk_text_buffer_get_iter_at_mark((GtkTextBuffer*)page->buffer, &iter, mark);
			gtk_text_iter_forward_chars(&iter,str->len);
			gtk_text_buffer_place_cursor((GtkTextBuffer*)page->buffer, &iter);

			g_free(command);
			g_string_free(str, true);
			pclose(fp);
        }
		g_strfreev(array);
    } else {
		dropTextFile = false;
    }
	gtk_drag_finish (context, true, true, time);
}

bool getSaveFile(bool isSave) {
	GtkWidget* dialog;
	bool       retval = false;

	dialog = gtk_file_chooser_dialog_new(
        _("Save File"), (GtkWindow*)window,
         GTK_FILE_CHOOSER_ACTION_SAVE,
         GTK_STOCK_CANCEL,
         GTK_RESPONSE_CANCEL,
         GTK_STOCK_SAVE,
         GTK_RESPONSE_ACCEPT,
         NULL
    );

	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER (dialog), TRUE);
	if (isSave == false) {
        if (saveFilePath != NULL) {
			gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), g_path_get_dirname(saveFilePath));
        }
		gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), saveFileName);
    } else if(isSave == true) {
        if (savePath!=NULL) {
			gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), g_path_get_dirname(savePath));
        }
		gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), "Untitled.1");
    } else {
		gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), "Untitled");
    }
	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		saveFilePath = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		saveFileName = g_path_get_basename(saveFilePath);
		retval = true;
    }

	gtk_widget_destroy(dialog);
	refreshMainWindow();
	return(retval);
}

char* doReplaceTags(char* str) {
	char* newstr = strdup(str);
	char* tagstr = NULL;

	const char*	tagfrom[] = {"&gt;", "&lt;", "&apos;", "&quot;"};
	const char*	tagto[]   = {">", "<", "'", "\""};

    bool flag = true;

	while (flag == true) {
        tagstr = sliceInclude(newstr, (char*)"<apply_tag name=\"bold", (char*)"\">", true, true);
		if (tagstr == NULL)
			flag = false;
		else
			replaceAllSlice(&newstr, tagstr, (char*)"\\fB");
    }

	flag   = true;
	tagstr = NULL;
	while (flag == true) {
		tagstr = sliceInclude(newstr, (char*)"<apply_tag name=\"italic", (char*)"\">", true, true);
		if (tagstr == NULL)
			flag = false;
        else
			replaceAllSlice(&newstr, tagstr, (char*)"\\fI");
    }

	replaceAllSlice(&newstr, (char*)"</apply_tag>", (char*)"\\fR");
	for (int j = 0; j < 4; j++) {
		replaceAllSlice(&newstr, (char*)tagfrom[j], (char*)tagto[j]);
    }
	g_free(str);
	return(newstr);
}

char* loadToString(pageStruct* page) {
	guint8* data;

	GtkTextIter start;
	GtkTextIter end;

    gsize length;
	char* ptr = NULL;

	GdkAtom	atom = gtk_text_buffer_register_serialize_tagset((GtkTextBuffer*)page->buffer, NULL);

	gtk_text_buffer_get_bounds((GtkTextBuffer*)page->buffer, &start, &end);
	data = gtk_text_buffer_serialize(
        (GtkTextBuffer*)page->buffer,
        (GtkTextBuffer*)page->buffer,
        atom,
        &start,
        &end,
        &length
    );
	ptr = sliceInclude((char*)&data[31], (char*)"<text>", (char*)"</text>", false, false);
	g_free(data);
	return(ptr);
}

void saveFile(GtkWidget* widget, gpointer data) {
	pageStruct* page;
	GtkTextIter start, end;

	gchar*   text;
	char*    ptr;
    char*    linePtr;
	char*    holdPtr;
    char*    xmldata = NULL;
    char*    zipcommand = NULL;
    char     startChar[2];
	int      numpages = gtk_notebook_get_n_pages(notebook);
	bool     lastWasNL = false;
	FILE*    fd = NULL;
	GString* str = g_string_new(NULL);
	

	if (savePath == NULL || data != NULL) {
		if (getSaveFile(true) == false) {
			return;
        }
		savePath = strdup(saveFilePath);
    }

	if (previewFile != NULL)
		 fd = previewFile;
	else fd = fopen(savePath, "w");
	if (fd != NULL) {
        fprintf(fd, ".TH \"%s\" \"%s\" \"%s\" \"%s\" \"%s\"\n", manName, manSection, manVersion, manAuthor, manCategory);

        ptr = text;
		startChar[1] = 0;

        for (int loop = 0; loop < numpages; loop++) {
            page = getPageStructPtr(loop);
			gtk_text_buffer_get_start_iter((GtkTextBuffer*)page->buffer, &start);
			gtk_text_buffer_get_end_iter((GtkTextBuffer*)page->buffer, &end);
			gtk_text_buffer_place_cursor((GtkTextBuffer*)page->buffer, &start);
			xmldata = loadToString(page);

			ptr = doReplaceTags(xmldata);
            g_string_assign(str, page->fileName);
			g_string_ascii_up(str);
			if (page->isSubsection == false)
                 fprintf(fd, ".SH \"%s\"\n", str->str);
            else fprintf(fd, ".SS \"%s\"\n", str->str);

            while(strlen(ptr) > 0) {
				startChar[0] = ptr[0];
				if (strcmp(startChar, "\n") != 0) {
					linePtr = sliceInclude(ptr, (char*)&startChar[0], (char*)"\n", true, false);
					if (linePtr == NULL && strlen(ptr) > 0) {
						fprintf(fd, "%s\n.br\n", ptr);
                        g_free(ptr);
						ptr = (char*)"";
					} else {
                        fprintf(fd, "%s\n.br\n", linePtr);
						g_free(linePtr);
                        linePtr = sliceInclude(ptr, (char*)&startChar[0], (char*)"\n", true, true);
						holdPtr = deleteSlice(ptr, linePtr);
						g_free(linePtr);
						g_free(ptr);
						ptr = holdPtr;
						lastWasNL = false;
					}
                } else {
					holdPtr = deleteSlice(ptr, (char*)"\n");
					g_free(ptr);
					ptr = holdPtr;

					if (lastWasNL == false) fprintf(fd, "\n");
                    lastWasNL = true;
                }
            }
        }
		fclose(fd);
		if (gzipPages == true) {
            asprintf(&zipcommand, "gzip --force %s", savePath);
			system(zipcommand);
			g_free(zipcommand);
        }
    }
}

void saveConverted(pageStruct* page) {
	FILE*   output;
	guint8* data;
	gsize   length;

	GtkTextIter start;
	GtkTextIter end;

	GdkAtom	atom = gtk_text_buffer_register_serialize_tagset((GtkTextBuffer*)page->buffer, NULL);
	gtk_text_buffer_get_bounds((GtkTextBuffer*)page->buffer, &start, &end);
	data = gtk_text_buffer_serialize(
        (GtkTextBuffer*)page->buffer,
        (GtkTextBuffer*)page->buffer,
        atom,
        &start,
        &end,
        &length
    );
	output = fopen(page->filePath, "wb");
	fwrite(data, sizeof(guint8), length, output);
	fclose(output);
}
/*
void saveManpage(GtkWidget* widget, gpointer data) {
	int   numpages = gtk_notebook_get_n_pages(notebook);
	FILE* fd = NULL;
	char* manifest;
	char* lowername;
	char* dirname;
	char* recenturi;

    pageStruct*	page;

	if (manFilePath == NULL) {
        if (getSaveFile(false) == false) {
            return;
        }
		if (g_str_has_suffix(saveFilePath, ".mpz"))
            manFilePath=strdup(saveFilePath);
        else
			asprintf(&manFilePath, "%s.mpz", saveFilePath);
    }

	for (int loop = 0; loop < numpages; loop++) {
		page = getPageStructPtr(loop);
        page->itsMe = true;
		saveConverted(page);
    }

	asprintf(&manifest, "%s/manifest", manFilename);
	fd = fopen(manifest, "w");
	if (fd != NULL) {
		fprintf(fd, "manname %s\n",     manName);
		fprintf(fd, "mansection %s\n",  manSection);
		fprintf(fd, "manversion %s\n",  manVersion);
		fprintf(fd, "manauthor %s\n",   manAuthor);
		fprintf(fd, "mancategory %s\n", manCategory);
		for (int loop = 0; loop < numpages; loop++) {
            page = getPageStructPtr(loop);
			if (page->isSubsection == false)
                 fprintf(fd, "file %s\n", page->fileName);
            else fprintf(fd, "subsection %s\n", page->fileName);
        }
		fclose(fd);
    }
	g_free(manifest);

	// Creates archive
	asprintf(&manifest, "xz -z %s -f %s .", manFilename, manFilePath);
	system(manifest);
	g_free(manifest);

	if (savePath != NULL) g_free(savePath);

	dirname   = g_path_get_dirname(manFilePath);
	lowername = g_ascii_strdown(manName, -1);
	asprintf(&savePath, "%s/%s.%s", dirname, lowername, manSection);
	g_free(dirname);
	g_free(lowername);
	recenturi = g_filename_to_uri(manFilePath, NULL, NULL);
	gtk_recent_manager_add_item(gtk_recent_manager_get_default(), recenturi);

	dirty = false;
	setSensitive();
}
*/
void saveAs(GtkWidget* widget, gpointer data) {
	if (manFilePath != NULL) {
        printf("%s\n", manFilePath);
		g_free(manFilePath);
		manFilePath = NULL;
    }
	saveFile(NULL, NULL);
}

pageStruct* makeNewPage(void) {
	pageStruct* page;

	page = (pageStruct*) malloc(sizeof(pageStruct));
	page->pane = gtk_vpaned_new();
	page->pageWindow  = (GtkScrolledWindow*)gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(
        GTK_SCROLLED_WINDOW(page->pageWindow),
        GTK_POLICY_AUTOMATIC,
        GTK_POLICY_AUTOMATIC
    );
	page->pageWindow2 = (GtkScrolledWindow*)gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(
        GTK_SCROLLED_WINDOW(page->pageWindow2),
        GTK_POLICY_AUTOMATIC,
        GTK_POLICY_AUTOMATIC
    );
	page->buffer = gtk_source_buffer_new(NULL);
	page->view   = (GtkSourceView*)gtk_source_view_new_with_buffer(page->buffer);
	gtk_text_buffer_set_modified((GtkTextBuffer*)page->buffer, false);

	g_signal_connect(G_OBJECT(page->view), "populate-popup", G_CALLBACK(populatePopupMenu), NULL);
	page->view2 = (GtkSourceView*)gtk_source_view_new_with_buffer(page->buffer);

	setFilePrefs(page->view);
	gtk_paned_add1(GTK_PANED(page->pane), (GtkWidget*)page->pageWindow);
	gtk_container_add(GTK_CONTAINER(page->pageWindow), (GtkWidget*)page->view);
	g_signal_connect(
        G_OBJECT(page->view),
        "button-release-event",
        G_CALLBACK(whatPane),
        (void*)1
    );

	page->isFirst = true;
	page->itsMe   = false;
	page->inTop   = true;
	page->isSplit = false;
	page->lang    = NULL;
	page->tabVbox = NULL;

	gtk_drag_dest_set(
        (GtkWidget*)page->view,
        GTK_DEST_DEFAULT_ALL,
        NULL,
        0,
        GDK_ACTION_COPY
    );
	gtk_drag_dest_add_uri_targets((GtkWidget*)page->view);
	gtk_drag_dest_add_text_targets((GtkWidget*)page->view);
	g_signal_connect_after(
        G_OBJECT(page->view),
        "drag-data-received",
        G_CALLBACK(dropText),
        (void*)page
    );
	gtk_text_buffer_set_modified(GTK_TEXT_BUFFER(page->buffer), false);
	g_signal_connect(
        G_OBJECT(page->buffer),
        "modified-changed",
        G_CALLBACK(makeDirty),
        (void*)page
    );
	gtk_widget_grab_focus((GtkWidget*)page->view);

	return(page);
}

void newManpage(GtkWidget* widget, gpointer data) {
	GtkWidget* dialog;
	GtkWidget* content_area;
	GtkWidget* label;
	GtkWidget* hbox;

    gint result;

	if (pageOpen == true) closePage(NULL, NULL);

	dialog = gtk_message_dialog_new(
        GTK_WINDOW(window),
        GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_QUESTION,
        GTK_BUTTONS_NONE,
        _("Create New Manpage")
    );

	gtk_dialog_add_buttons(
        (GtkDialog*)dialog,
        GTK_STOCK_CANCEL,
        GTK_RESPONSE_CANCEL,
        GTK_STOCK_OK,
        GTK_RESPONSE_YES,
        NULL
    );
	gtk_window_set_title(GTK_WINDOW(dialog), _("Details"));

	content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

	hbox = creatNewBox(NEWHBOX,false,0);

	nameBox = gtk_entry_new();
	label   = gtk_label_new(_("Name\t"));
	gtk_box_pack_start(GTK_BOX(hbox), label,   true, true, 0);
	gtk_box_pack_start(GTK_BOX(hbox), nameBox, true, true, 0);
	gtk_container_add(GTK_CONTAINER(content_area), hbox);

	hbox = creatNewBox(NEWHBOX, false, 0);

	sectionBox = gtk_entry_new();
	label      = gtk_label_new(_("Section\t"));
	gtk_box_pack_start(GTK_BOX(hbox), label,      true, true, 0);
	gtk_box_pack_start(GTK_BOX(hbox), sectionBox, true, true, 0);
	gtk_container_add(GTK_CONTAINER(content_area), hbox);

	hbox = creatNewBox(NEWHBOX, false, 0);

	versionBox = gtk_entry_new();
	label      = gtk_label_new(_("Version\t"));
	gtk_box_pack_start(GTK_BOX(hbox), label,      true, true, 0);
	gtk_box_pack_start(GTK_BOX(hbox), versionBox, true, true, 0);
	gtk_container_add(GTK_CONTAINER(content_area),hbox);

	hbox = creatNewBox(NEWHBOX, false, 0);

	authorBox = gtk_entry_new();
	label     = gtk_label_new(_("Author\t"));
	gtk_box_pack_start(GTK_BOX(hbox), label,     true, true, 0);
	gtk_box_pack_start(GTK_BOX(hbox), authorBox, true, true, 0);
	gtk_container_add(GTK_CONTAINER(content_area), hbox);

	hbox = creatNewBox(NEWHBOX, false, 0);

	categoryBox = gtk_entry_new();
	label       = gtk_label_new(_("Category\t"));
	gtk_box_pack_start(GTK_BOX(hbox), label,       true, true, 0);
	gtk_box_pack_start(GTK_BOX(hbox), categoryBox, true, true, 0);
	gtk_container_add(GTK_CONTAINER(content_area), hbox);


	gtk_widget_show_all(content_area);
	result = gtk_dialog_run(GTK_DIALOG(dialog));

	if (result == GTK_RESPONSE_YES) {
		sprintf(tFileName, "%s", "/tmp/mpe-gtk2XXXXXX");
		manFilename = mkdtemp(tFileName);
		manName     = strdup(gtk_entry_get_text((GtkEntry*)nameBox));
		manSection  = strdup(gtk_entry_get_text((GtkEntry*)sectionBox));
		manVersion  = strdup(gtk_entry_get_text((GtkEntry*)versionBox));
		manAuthor   = strdup(gtk_entry_get_text((GtkEntry*)authorBox));
		manCategory = strdup(gtk_entry_get_text((GtkEntry*)categoryBox));
		pageOpen    = true;
		g_mkdir_with_parents(manFilename, 493);
    }
	gtk_widget_destroy(dialog);
}

char* getNewSectionName(char* name) {
	GtkWidget* dialog;
	GtkWidget* content_area;
	GtkWidget* entrybox;
    GtkWidget* checkbox;
    gint       result;
	char*      retval = NULL;
	

	dialog = gtk_message_dialog_new(
        GTK_WINDOW(window),
        GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_QUESTION,
        GTK_BUTTONS_NONE,
        _("Enter Section Name")
    );

	gtk_dialog_add_buttons(
        (GtkDialog*)dialog,
        GTK_STOCK_CANCEL,
        GTK_RESPONSE_CANCEL,
        GTK_STOCK_OK,
        GTK_RESPONSE_YES,
        NULL
    );
	gtk_window_set_title(GTK_WINDOW(dialog), _("Section"));

	content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	entrybox     = gtk_entry_new();
	if (name != NULL) {
		gtk_entry_set_text((GtkEntry*)entrybox, name);
    }
	gtk_entry_set_activates_default((GtkEntry*)entrybox, true);
	gtk_dialog_set_default_response((GtkDialog*)dialog, GTK_RESPONSE_YES);
	gtk_container_add(GTK_CONTAINER(content_area), entrybox);
	checkbox = gtk_check_button_new_with_label(_("Make Subsection"));
	gtk_toggle_button_set_active((GtkToggleButton*)checkbox, isSubsection);
	gtk_container_add(GTK_CONTAINER(content_area), checkbox);

	gtk_widget_show_all(content_area);
	result = gtk_dialog_run(GTK_DIALOG(dialog));

	if (result == GTK_RESPONSE_YES) {
		isSubsection = gtk_toggle_button_get_active((GtkToggleButton*)checkbox);
        retval = strdup(gtk_entry_get_text((GtkEntry*)entrybox));
    }
	gtk_widget_destroy(dialog);

	return(retval);
}

void newSection(GtkWidget* widget, gpointer data) {
	GtkTextIter iter;
	GtkWidget*  label;
	pageStruct* page;
	char*       retval = NULL;
	GString*    str;

	if (data == NULL) {
        isSubsection = false;
		retval = getNewSectionName(NULL);
    } else {
        retval = strdup((char*)data);
    }
	if (retval != NULL) {
		page = makeNewPage();
		page->tabVbox = creatNewBox(NEWVBOX, true, 4);
		str = g_string_new(retval);
        if (isSubsection == false)
			 g_string_ascii_up(str);
		else g_string_ascii_down(str);

		page->fileName = g_string_free(str, false);
		asprintf(&page->filePath, "%s/%s", manFilename, page->fileName);

        label = makeNewTab(page->fileName, NULL, page);

		/* move cursor to the beginning */
		gtk_text_buffer_get_start_iter(GTK_TEXT_BUFFER(page->buffer), &iter);
		gtk_text_buffer_place_cursor(GTK_TEXT_BUFFER(page->buffer), &iter);
		//
		// Connect to notebook
		//
		gtk_box_pack_start ((GtkBox*)page->tabVbox, (GtkWidget*)page->pane, true, true, 0);

		g_object_set_data(G_OBJECT(page->tabVbox), "pagedata", (gpointer)page);

		gtk_notebook_append_page(notebook,page->tabVbox, label);
		gtk_notebook_set_tab_reorderable(notebook,page->tabVbox, true);
		gtk_notebook_set_current_page(notebook,currentPage);

		currentPage++;
		gtk_widget_show_all((GtkWidget*)notebook);
		page->isSubsection = isSubsection;
		if (data != NULL) {
			importPage = page;
        }
	}
}

void loadBuffer(pageStruct* page) {
	GtkTextIter iter;

	gchar*  buffer = NULL;
	long    filelen;
	GdkAtom atom = gtk_text_buffer_register_deserialize_tagset((GtkTextBuffer*)page->buffer, NULL);

	g_file_get_contents(page->filePath,&buffer, (gsize*)&filelen, NULL);
	gtk_text_buffer_get_start_iter((GtkTextBuffer*)page->buffer, &iter);
	gtk_text_buffer_deserialize_set_can_create_tags((GtkTextBuffer*)page->buffer, atom, true);
	gtk_text_buffer_deserialize(
        (GtkTextBuffer*)page->buffer,
        (GtkTextBuffer*)page->buffer,
        atom,
        &iter,
        (const guint8*)buffer,
        filelen,
        NULL
    );
}

void openConvertedFile(char* filepath) {
	GtkWidget*  label;
	gchar*      filename = g_path_get_basename(filepath);
	pageStruct* page;
	char*       str = NULL;

	if (!g_file_test(filepath, G_FILE_TEST_EXISTS)) return;

	page = makeNewPage();
	page->tabVbox      = creatNewBox(NEWVBOX, true, 4);
	page->filePath     = strdup(filepath);
	page->fileName     = strdup(filename);
	page->isSubsection = isSubsection;

	label = makeNewTab(page->fileName, page->filePath, page);

	gtk_source_buffer_set_highlight_syntax(page->buffer, false);
	gtk_source_buffer_set_highlight_matching_brackets(page->buffer, false);
	gtk_source_buffer_begin_not_undoable_action(page->buffer);

    loadBuffer(page);
	setUnderlining(page);
	gtk_source_buffer_end_not_undoable_action(page->buffer);

	g_free(filename);
	g_free(str);
	//
	// Connect to notebook
	//
	gtk_container_add(GTK_CONTAINER(page->tabVbox), GTK_WIDGET(page->pane));
	g_object_set_data(G_OBJECT(page->tabVbox), "pagedata", (gpointer)page);

	gtk_notebook_append_page(notebook,page->tabVbox, label);
	gtk_notebook_set_tab_reorderable(notebook,page->tabVbox, true);
	gtk_notebook_set_current_page(notebook,currentPage);

	currentPage++;
	gtk_widget_grab_focus((GtkWidget*)page->view);
	gtk_text_buffer_set_modified(GTK_TEXT_BUFFER(page->buffer), false);
	gtk_widget_show_all((GtkWidget*)notebook);
}

void deleteSection(GtkWidget* widget, gpointer data) {
	pageStruct* page = getPageStructPtr(-1);

	if (page == NULL) {
		return;
    }
	if (yesNo((char*) _("Do you want to permanently delete"), page->fileName) == GTK_RESPONSE_YES) {
		unlink(page->filePath);
		closeTab(NULL, (void*)1);
        makeDirty(NULL, NULL);
    }
}

void renameSection(GtkWidget* widget, gpointer data) {
	pageStruct* page = (pageStruct*)data;
	char*       retval  = NULL;
	char*       command = NULL;
	GString*    str;

	isSubsection = page->isSubsection;
	retval = getNewSectionName(page->fileName);
	if (retval != NULL) {
		page->isSubsection = isSubsection;
		str = g_string_new(retval);
		if (page->isSubsection == false)
		     g_string_ascii_up(str);
        else g_string_ascii_down(str);

        asprintf(&command, "mv \"%s\" \"%s/%s\"", page->filePath, manFilename, str->str);
		g_free(page->fileName);
		g_free(page->filePath);

		page->fileName = g_string_free(str, false);
		asprintf(&page->filePath, "%s/%s", manFilename, page->fileName);
		system(command);
		gtk_label_set_text((GtkLabel*)page->tabName, page->fileName);
		makeDirty(NULL, (void*)page);
		g_free(retval);
    }
}

void importSection(char* line) {
	char* name;

	if(strncmp(line, "@section@", 9) == 0)
		 isSubsection = true;
	else isSubsection = false;

	name = strdup(g_strchug((char*)&line[10]));

	replaceAllSlice(&name, (char*)"\e[1m", (char*)"");
	replaceAllSlice(&name, (char*)"\e[0m", (char*)"");
	replaceAllSlice(&name, (char*)"\n",    (char*)"");

	newSection(NULL, name);
}

GtkTextTag*	getNamedTag(int tagType) {
	char tagname[64];

	GtkTextTag*      tag = NULL;
	GtkTextTagTable* tagtable = gtk_text_buffer_get_tag_table((GtkTextBuffer*)importPage->buffer);

	switch (tagType) {
		case BOLD:
			tagboldnum++;
			sprintf((char*)&tagname, "bold-%i", tagboldnum);
			tag = gtk_text_tag_table_lookup(tagtable, tagname);
			while (tag != NULL) {
                tagboldnum++;
				sprintf((char*)&tagname, "bold-%i", tagboldnum);
				tag = gtk_text_tag_table_lookup(tagtable, tagname);
            }
			return(gtk_text_buffer_create_tag(
                    (GtkTextBuffer*)importPage->buffer,
                    tagname,
                    "weight",
                    PANGO_WEIGHT_BOLD,
                    NULL)
                  );
			break;
		case ITALIC:
			tagitalicnum++;
			sprintf((char*)&tagname, "italic-%i", tagitalicnum);
			tag = gtk_text_tag_table_lookup(tagtable, tagname);
			while (tag != NULL) {
                tagitalicnum++;
				sprintf((char*)&tagname, "bold-%i", tagitalicnum);
				tag=gtk_text_tag_table_lookup(tagtable, tagname);
            }
			if (useUnderline == true)
				 return(gtk_text_buffer_create_tag(
                            (GtkTextBuffer*)importPage->buffer,
                            tagname,
                            "underline",
                            PANGO_UNDERLINE_SINGLE,
                            NULL)
                        );
			else return(gtk_text_buffer_create_tag(
                            (GtkTextBuffer*)importPage->buffer,
                            tagname,
                            "style",
                            PANGO_STYLE_ITALIC,
                            NULL)
                        );
		break;
    }
	return(NULL);
}

void replaceTags(void) {
	GtkTextIter start;
	GtkTextIter endtag;
	GtkTextIter starttag;
	GtkTextIter endtag2;
	GtkTextIter starttag2;
	GtkTextTag* tag = NULL;

	bool flag       = true;
    bool noendfound = true;

	const char* texttags[]    = {BOLDESC, ITALICESC};
	const char* endtexttags[] = {NORMALESC, "\n"};

	int numstarttags = 2;
	int numendtags   = 2;
	int nltag = numendtags - 1;

	GtkTextSearchFlags flags = GTK_TEXT_SEARCH_TEXT_ONLY;

	flag = true;
	for (int j = 0; j < numstarttags; j++) {
        while (flag == true) {
            gtk_text_buffer_get_start_iter((GtkTextBuffer*)importPage->buffer, &start);
            if (gtk_text_iter_forward_search(&start, texttags[j], flags, &starttag, &endtag, NULL)) {
                noendfound = true;
				for (int k = 0; k < numendtags; k++) {
                    if (gtk_text_iter_forward_search(&endtag, endtexttags[k], flags, &starttag2, &endtag2, NULL)) {
						noendfound = false;
                        tag = getNamedTag(j);

						gtk_text_buffer_apply_tag((GtkTextBuffer*)importPage->buffer, tag, &endtag, &starttag2);
						gtk_text_buffer_get_start_iter((GtkTextBuffer*)importPage->buffer, &start);
						gtk_text_iter_forward_search(&start, texttags[j], flags, &starttag, &endtag, NULL);
						gtk_text_buffer_delete((GtkTextBuffer*)importPage->buffer, &starttag, &endtag);
						if (k != nltag) {
							gtk_text_iter_forward_search(&starttag, endtexttags[k], flags, &starttag2, &endtag2, NULL);
							gtk_text_buffer_delete((GtkTextBuffer*)importPage->buffer, &starttag2, &endtag2);
                        }
					    break;
                    }
                }
				if (noendfound == true) {
                    gtk_text_buffer_get_end_iter((GtkTextBuffer*)importPage->buffer, &endtag2);
					gtk_text_buffer_apply_tag((GtkTextBuffer*)importPage->buffer, tag, &starttag, &endtag2);
					gtk_text_buffer_delete((GtkTextBuffer*)importPage->buffer, &starttag, &endtag);
					break;
				}
            } else 	break;
        }
    }
}

char* getLineFromString(char* bigStr) {
	char* nl = strchr(bigStr, '\n');
	char* retval = NULL;
	char  startchar[2];

	startchar[0] = bigStr[0];
	startchar[1] = 0;

	if (nl != NULL)
		 retval = sliceInclude(bigStr, (char*)&startchar[0], (char*)"\n", true, true);
	else retval = strdup(bigStr);

	return(retval);
}

char* cleanText(char* text) {
    char*        line;
	char*        data    = text;
	GString*     srcstr  = g_string_new(data);
	GString*     deststr = g_string_new(NULL);
	unsigned int charpos = 0;
	int          numSpaces = 0;

	line = getLineFromString(srcstr->str);
	while (line[numSpaces] == ' ') {
		numSpaces++;
    }
	while (charpos<srcstr->len) {
        if (srcstr->str[charpos] == '\n') {
			g_string_append(deststr, "\n");
			charpos++;
		} else {
			line = getLineFromString((char*)&srcstr->str[charpos]);
			g_string_append(deststr, (char*)&line[numSpaces]);
			charpos = charpos + strlen(line);
			g_free(line);
        }
	}
	g_string_free(srcstr, true);
	return(g_string_free(deststr, false));
}

void openManpage(GtkWidget* widget, gpointer data) {
	GtkWidget* dialog   = NULL;
	char*      filename = NULL;

    GtkFileFilter* filterZipped = gtk_file_filter_new();
	GtkFileFilter* filterPlain  = gtk_file_filter_new();

	gtk_file_filter_add_pattern(filterZipped, "*.xz");
    gtk_file_filter_add_pattern(filterZipped, "*.gz");
    gtk_file_filter_set_name(filterZipped, _("Copmressed man pages (*.xz, *.gz)"));
        
    gtk_file_filter_add_pattern(filterPlain, "*.1");
    gtk_file_filter_add_pattern(filterPlain, "*.2");
    gtk_file_filter_add_pattern(filterPlain, "*.3");
    gtk_file_filter_add_pattern(filterPlain, "*.4");
    gtk_file_filter_add_pattern(filterPlain, "*.5");
    gtk_file_filter_add_pattern(filterPlain, "*.6");
    gtk_file_filter_add_pattern(filterPlain, "*.7");
    gtk_file_filter_add_pattern(filterPlain, "*.8");
    gtk_file_filter_add_pattern(filterPlain, "*.9");
    gtk_file_filter_set_name(filterPlain, _("Plain man pages (*.1...*.9)"));

    dialog = gtk_file_chooser_dialog_new(
        _("Open Manpage"),
        NULL,
        GTK_FILE_CHOOSER_ACTION_OPEN,
        GTK_STOCK_CANCEL,
        GTK_RESPONSE_CANCEL,
        GTK_STOCK_OPEN,
        GTK_RESPONSE_ACCEPT,
        NULL
    );

    gtk_file_chooser_add_filter((GtkFileChooser*)dialog, filterZipped);
    gtk_file_chooser_add_filter((GtkFileChooser*)dialog, filterPlain);

    if (gtk_dialog_run(GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
	} else {
		gtk_widget_destroy(dialog);
		return;
    }

	closePage(NULL, NULL);
    openFile(filename);
    if (dialog != NULL) gtk_widget_destroy (dialog);
}

void openFile(char* file) {
    char*  contents;
	char*  start = NULL;
	char*  end   = NULL;
    char*  strok = NULL;
	char*  sect  = NULL;
	char*  ptr;
	char*  recenturi;
	char*  props;
	char** propargs;
	char   buffer[2048]        = {0,};
    char   nameBuffer[2048]    = {0,};
	char   commandBuffer[2048] = {0,};
	int    numprops;
	long   len;
    
    GtkTextIter	iter;
	GString*    str = g_string_new(NULL);
    FILE*       fp;

	sprintf(tFileName, "%s", "/tmp/mpe-gtk2XXXXXX");
	manFilename = mkdtemp(tFileName);
	g_mkdir_with_parents(manFilename, 493);

	if (g_str_has_suffix(file, ".xz")) {
		sprintf(nameBuffer, "xz -dck %s|sed -n /^.TH/p", file);
#ifndef _USENROFF_
		sprintf(
            commandBuffer,
            "xz -dck %s|sed 's/^\\(\\.S[Hh]\\)/\\1 @SECTION@/g;s/^\\(\\.S[Ss]\\)/\\1 @section@/g'|GROFF_SGR=1 MANWIDTH=2000 MAN_KEEP_FORMATTING=\"1\" man -l --no-justification --no-hyphenation -|head -n -4",
            file
        );
#else
		sprintf(
            commandBuffer,
            "gunzip --stdout %s|sed '1i\\.ll 100'|sed 's/^\\.S[Hh] \\(.*\\)$/@SECTION@ \\1\\n/g'|sed 's/^@SECTION@ \"\\(.*\\)\"/@SECTION@ \\1/g'|sed 's/^.IR \\(.*\\)/\\\\fI\\1\\\\fR/g;s/^.B \\(.*\\)/\\\\fB\\1\\\\fR/g;s/\\.PP/\\n/g;s/\\.IP/  \\n/g'|nroff|head -n -4",
            file
        );
#endif // _USENROFF_
	} else if (g_str_has_suffix(file, ".gz")) {
        sprintf(nameBuffer, "gunzip --stdout %s|sed -n /^.TH/p", file);
#ifndef _USENROFF_
		sprintf(
            commandBuffer,
            "gunzip --stdout %s|sed 's/^\\(\\.S[Hh]\\)/\\1 @SECTION@/g;s/^\\(\\.S[Ss]\\)/\\1 @section@/g'|GROFF_SGR=1 MANWIDTH=2000 MAN_KEEP_FORMATTING=\"1\" man -l --no-justification --no-hyphenation -|head -n -4",
            file
        );
#else
		sprintf(
            commandBuffer,
            "gunzip --stdout %s|sed '1i\\.ll 100'|sed 's/^\\.S[Hh] \\(.*\\)$/@SECTION@ \\1\\n/g'|sed 's/^@SECTION@ \"\\(.*\\)\"/@SECTION@ \\1/g'|sed 's/^.IR \\(.*\\)/\\\\fI\\1\\\\fR/g;s/^.B \\(.*\\)/\\\\fB\\1\\\\fR/g;s/\\.PP/\\n/g;s/\\.IP/  \\n/g'|nroff|head -n -4",
            file
        );
#endif // _USENROFF_
    } else {
		sprintf(nameBuffer, "cat %s|sed -n /^.TH/p", file);
#ifndef _USENROFF_
		sprintf(
            commandBuffer,
            "cat %s|sed 's/^\\(\\.S[Hh]\\)/\\1 @SECTION@/g;s/^\\(\\.S[Ss]\\)/\\1 @section@/g'|GROFF_SGR=1 MANWIDTH=2000 MAN_KEEP_FORMATTING=\"1\" man -l --no-justification --no-hyphenation -|head -n -4",
            file
        );
#else
		sprintf(
            commandBuffer,
            "cat %s|sed '1i\\.ll 100'|sed 's/^\\.S[Hh] \\(.*\\)$/@SECTION@ \\1\\n/g'|sed 's/^@SECTION@ \"\\(.*\\)\"/@SECTION@ \\1/g'|sed 's/^.IR \\(.*\\)/\\\\fI\\1\\\\fR/g;s/^.B \\(.*\\)/\\\\fB\\1\\\\fR/g;s/\\.PP/\\n/g;s/\\.IP/  \\n/g'|nroff|head -n -4",
            file
        );
#endif // _USENROFF_
    }

	fp = popen(commandBuffer, "r");
	if (fp != NULL) {
        while (fgets(buffer, 2048, fp)) {
			g_string_append(str, buffer);
        }
		pclose(fp);
    }
    //
    // Get properties
    //
	buffer[0] = 0;
	fp = popen(nameBuffer, "r");
	if (fp != NULL) {
        fgets((char*)&buffer[0], 2048, fp);
		pclose(fp);
    }

	if (strlen(buffer) == 0) {
		char *name = NULL, *date = NULL, *tsec = NULL;
		char sec;

        if (g_str_has_suffix(file, ".xz"))
			 sprintf(nameBuffer, "xz -dck %s", file);
        else if (g_str_has_suffix(file, ".gz"))
             sprintf(nameBuffer, "gunzip --stdout %s", file);
		else sprintf(nameBuffer, "cat %s", file);

        buffer[0] = 0;
		fp = popen(nameBuffer, "r");
		if (fp != NULL) {
			while (fgets(buffer, 2048, fp)) {
                if (name == NULL) name = sliceBetween((char*)&buffer[0], (char*)".Dt ", (char*)" ");
				if (date == NULL) date = sliceBetween((char*)&buffer[0], (char*)".Dd ", (char*)"\n");
				if (tsec == NULL) tsec = sliceBetween((char*)&buffer[0], (char*)".Dt ", (char*)"\n");
            }
			pclose(fp);
        }

		if (name == NULL) name = strdup("");
		if (date == NULL) date = strdup("");
		if (tsec == NULL) {
            tsec = strdup("");
            sec  = ' ';
        } else {
			sec = tsec[strlen(tsec)-1];
        }
        asprintf(&props, "\"%s\" \"%c\" \"%s\" \"\" \"\"", name, sec, date);
		free(name);
		free(date);
		free(tsec);
	} else {
		props = sliceBetween((char*)&buffer[0], (char*)".TH ", (char*)"\n");
    }
	contents = g_string_free(str, false);
	if (g_utf8_validate(contents, -1, NULL) == false)
		 strok = g_locale_to_utf8(contents, -1, NULL, NULL, NULL);
	else strok = strdup(contents);

	g_free(contents);
	contents = strok;
	//
	// Clean bold/italic tags
	//
	replaceAllSlice(&contents, (char*)"\x1b[22m", (char*)"\x1b[0m");
	replaceAllSlice(&contents, (char*)"\x1b[24m", (char*)"\x1b[0m");
	replaceAllSlice(&contents, (char*)"\x1b[4m\x1b[22m", (char*)"\x1b[22m\x1b[4m");

	ptr = contents;
	if (props != NULL) {
        if (manName != NULL) {
			g_free(manName);
			manName = NULL;
		}
		if (manSection != NULL) {
			g_free(manSection);
			manSection = NULL;
		}
		if (manVersion != NULL) {
			g_free(manVersion);
			manVersion = NULL;
		}
		if (manAuthor != NULL) {
			g_free(manAuthor);
			manAuthor = NULL;
		}
		if (manCategory != NULL) {
            g_free(manCategory);
            manCategory = NULL;
		}

		g_shell_parse_argv(props, &numprops, &propargs, NULL);
		if (numprops>0) manName     = strdup(propargs[0]);
		if (numprops>1) manSection  = strdup(propargs[1]);
		if (numprops>2) manVersion  = strdup(propargs[2]);
		if (numprops>3) manAuthor   = strdup(propargs[3]);
		if (numprops>4) manCategory = strdup(propargs[4]);
		g_strfreev(propargs);
    }

	while (true) {
		start = strcasestr(ptr, "@SECTION@");
		if (start == NULL) {
			break;
        }
		end = strcasestr((char*)&start[9], "@SECTION@");

		if(end == NULL) {
			sect = strdup(start);
		} else {
			len  = (long)end - (long)start;
			sect = strndup(start, len);
        }
		ptr = sliceCaseInclude(sect, (char*)"@SECTION@", (char*)"\n", true, true);

		importSection(strdup(ptr));
		replaceAllSlice(&sect, ptr, (char*)"");
		replaceAllSlice(&sect, (char*)"\\-", (char*)"-");

		sect = cleanText(sect);
		replaceAllSlice(&sect, (char*)"\x7f", (char*)"");
        gtk_source_buffer_begin_not_undoable_action(importPage->buffer);

		gtk_text_buffer_get_start_iter((GtkTextBuffer*)importPage->buffer, &iter);
		gtk_text_buffer_insert((GtkTextBuffer*)importPage->buffer, &iter, (char*)sect, -1);

		replaceTags();

		gtk_source_buffer_end_not_undoable_action(importPage->buffer);
		gtk_text_buffer_set_modified((GtkTextBuffer*)importPage->buffer, false);

        asprintf(&importPage->filePath, "%s/%s", manFilename, importPage->fileName);

        if (end == NULL) break;

        ptr = end;
    }

	recenturi = g_filename_to_uri(file, NULL, NULL);
	gtk_recent_manager_add_item(gtk_recent_manager_get_default(), recenturi);
	g_free(recenturi);
	gtk_window_set_title((GtkWindow*)window, file);

	g_free(file);
	g_free(contents);
	free(props);

	pageOpen = true;
	dirty    = false;
	setSensitive();
	refreshMainWindow();
}
