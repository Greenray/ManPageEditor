/*
 *
 * K.D.Hedger 2013 <kdhedger68713@gmail.com>
 *
 * callbacks.cpp
 *
*/

#include <gtk/gtk.h>
#include <gtksourceview/gtksourceiter.h>
//#include <unique/unique.h>
#include <gtksourceview/gtksourceprintcompositor.h>

#include "config.h"
#ifdef BUILDDOCVIEWER
#include <webkit/webkit.h>
#endif

#include "globals.h"
#include "files.h"
#include "guis.h"
#include "navcallbacks.h"
#include "searchcallbacks.h"
#include "spellcheck.h"

GtkWidget*			tabMenu;
char				defineText[1024];
GtkPrintSettings*	settings=NULL;

void showHideWidget(GtkWidget* widget,bool show)
{
	if(show==false)
		gtk_widget_hide(widget);
	else
		gtk_widget_show(widget);
}

void refreshMainWindow(void)
{
	gtk_widget_show_all(window);
	showHideWidget(lineNumberWidget,showJumpToLine);
	showHideWidget(liveSearchWidget,showLiveSearch);
}

int yesNo(char* question,char* file)
{
	GtkWidget*	dialog;
	int			result;

	dialog=gtk_message_dialog_new(GTK_WINDOW(window),GTK_DIALOG_DESTROY_WITH_PARENT,GTK_MESSAGE_WARNING,GTK_BUTTONS_NONE,"%s %s",question,file);

	gtk_dialog_add_buttons((GtkDialog*)dialog,GTK_STOCK_YES,GTK_RESPONSE_YES,GTK_STOCK_NO,GTK_RESPONSE_CANCEL,NULL);
	gtk_window_set_title(GTK_WINDOW(dialog),"What Do You Want To Do?");

	result=gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);

	return(result);
}

void doOpenFile(GtkWidget* widget,gpointer data)
{
	GtkWidget*	dialog;
	char*		filename;
	GSList*		filenames;
	GSList*		thisnext;
	
	dialog=gtk_file_chooser_dialog_new("Open File",NULL,GTK_FILE_CHOOSER_ACTION_OPEN,GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,NULL);
	gtk_file_chooser_set_select_multiple((GtkFileChooser*)dialog,true);
	if (gtk_dialog_run(GTK_DIALOG (dialog))==GTK_RESPONSE_ACCEPT)
		{
			filenames=gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dialog));
			thisnext=filenames;
			while(thisnext!=NULL)
				{
					filename=(char*)thisnext->data;
					openFile(filename,0);
					g_free (filename);
					thisnext=thisnext->next;
				}
			g_slist_free(filenames);
		}
	gtk_widget_destroy (dialog);
	refreshMainWindow();	
}

int show_question(char* filename)
{
	GtkWidget*	dialog;
	gint		result;
	char*		message;

	asprintf(&message,"Save file %s before closing?",filename);
	dialog=gtk_message_dialog_new(GTK_WINDOW(window),GTK_DIALOG_DESTROY_WITH_PARENT,GTK_MESSAGE_WARNING,GTK_BUTTONS_NONE,message);

	gtk_dialog_add_buttons((GtkDialog*)dialog,GTK_STOCK_SAVE,GTK_RESPONSE_YES,GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,GTK_STOCK_NO,GTK_RESPONSE_NO,NULL);
	gtk_window_set_title(GTK_WINDOW(dialog),"Warning unsaved data!");

	result=gtk_dialog_run(GTK_DIALOG(dialog));

	gtk_widget_destroy(dialog);
	g_free(message);
	return(result);
}

void setSensitive(void)
{
	pageStruct*		page=getPageStructPtr(currentTabNumber);
	const gchar*	text;
	char*			newlabel;
	int				offset=0;

	if(page==NULL)
		{
//toolbar
			gtk_widget_set_sensitive((GtkWidget*)undoButton,false);
			gtk_widget_set_sensitive((GtkWidget*)redoButton,false);
			gtk_widget_set_sensitive((GtkWidget*)saveButton,false);
//menu
			gtk_widget_set_sensitive((GtkWidget*)undoMenu,false);
			gtk_widget_set_sensitive((GtkWidget*)redoMenu,false);
			gtk_widget_set_sensitive((GtkWidget*)saveMenu,false);
			gtk_widget_set_sensitive((GtkWidget*)saveAsMenu,false);



			gtk_widget_set_sensitive((GtkWidget*)menuprint,false);
			gtk_widget_set_sensitive((GtkWidget*)menuclose,false);
			gtk_widget_set_sensitive((GtkWidget*)menucloseall,false);
			gtk_widget_set_sensitive((GtkWidget*)menusaveall,false);
			gtk_widget_set_sensitive((GtkWidget*)menurevert,false);
		}
	else
		{
			text=gtk_label_get_text((GtkLabel*)page->tabName);
//toolbar
			gtk_widget_set_sensitive((GtkWidget*)undoButton,gtk_source_buffer_can_undo(page->buffer));
			gtk_widget_set_sensitive((GtkWidget*)redoButton,gtk_source_buffer_can_redo(page->buffer));
			gtk_widget_set_sensitive((GtkWidget*)saveButton,gtk_text_buffer_get_modified(GTK_TEXT_BUFFER(page->buffer)));

//menu
			gtk_widget_set_sensitive((GtkWidget*)undoMenu,gtk_source_buffer_can_undo(page->buffer));
			gtk_widget_set_sensitive((GtkWidget*)redoMenu,gtk_source_buffer_can_redo(page->buffer));
			gtk_widget_set_sensitive((GtkWidget*)saveMenu,gtk_text_buffer_get_modified(GTK_TEXT_BUFFER(page->buffer)));
//tab
			gtk_widget_set_sensitive((GtkWidget*)saveButton,gtk_text_buffer_get_modified(GTK_TEXT_BUFFER(page->buffer)));
			if(text[0]=='*')
				offset=1;

			if(gtk_text_buffer_get_modified(GTK_TEXT_BUFFER(page->buffer))==true)
				asprintf(&newlabel,"*%s",&text[offset]);
			else
				newlabel=strdup(&text[offset]);

			gtk_label_set_text((GtkLabel*)page->tabName,(const gchar*)newlabel);
			g_free(newlabel);


			gtk_widget_set_sensitive((GtkWidget*)saveAsMenu,true);
			gtk_widget_set_sensitive((GtkWidget*)menuprint,true);
			gtk_widget_set_sensitive((GtkWidget*)menuclose,true);
			gtk_widget_set_sensitive((GtkWidget*)menucloseall,true);
			gtk_widget_set_sensitive((GtkWidget*)menusaveall,true);
			gtk_widget_set_sensitive((GtkWidget*)menurevert,true);
			gtk_widget_show_all(page->tabName);
		}
}

bool closingAll=false;

void closeTab(GtkWidget* widget,gpointer data)
{
	long		thispage;
	int			result;
	pageStruct*	page;

	if(closingAll==true)
		thispage=0;
	else
		{
			if(data==NULL)
				thispage=gtk_notebook_get_current_page(notebook);
			else
				thispage=gtk_notebook_page_num(notebook,(GtkWidget *)data);
		}

	closingAll=false;
	page=getPageStructPtr(thispage);
	if(page==NULL)
		return;

	if(gtk_text_buffer_get_modified((GtkTextBuffer*)page->buffer))
		{
			result=show_question(page->fileName);
			switch(result)
				{
					case GTK_RESPONSE_YES:
						if(saveFile(NULL,NULL)==false)
							return;
						break;
					case GTK_RESPONSE_NO:
						break;
					default:
						return;
						break;
				}
		}

	if(page->filePath!=NULL)
		g_free(page->filePath);
	if(page->fileName!=NULL)
		g_free(page->fileName);
	if(page->gFile!=NULL)
		g_object_unref(page->gFile);
	if(page->monitor!=NULL)
		g_object_unref(page->monitor);
	if(page!=NULL)
		g_free(page);
	currentPage--;
	gtk_notebook_remove_page(notebook,thispage);
	setSensitive();
}

void closeAllTabs(GtkWidget* widget,gpointer data)
{
	int	numtabs=gtk_notebook_get_n_pages(notebook);

	for(int loop=0;loop<numtabs;loop++)
		{
			closingAll=true;
			closeTab(NULL,0);
		}
}

void switchPage(GtkNotebook *notebook,gpointer arg1,guint thispage,gpointer user_data)
{
	pageStruct*	page;

	if(arg1==NULL)
		return;

	page=(pageStruct*)g_object_get_data((GObject*)arg1,"pagedata");
	if(page==NULL)
		return;

	currentTabNumber=thispage;



	page->isFirst=true;

	gtk_window_set_title((GtkWindow*)window,page->fileName);
	refreshMainWindow();

	setSensitive();

}

void copyToClip(GtkWidget* widget,gpointer data)
{
	pageStruct*	page=getPageStructPtr(-1);
	gtk_text_buffer_copy_clipboard((GtkTextBuffer*)page->buffer,gtk_clipboard_get(GDK_SELECTION_CLIPBOARD));
}

void cutToClip(GtkWidget* widget,gpointer data)
{
	pageStruct*	page=getPageStructPtr(-1);
	gtk_text_buffer_cut_clipboard((GtkTextBuffer*)page->buffer,gtk_clipboard_get(GDK_SELECTION_CLIPBOARD),true);
	setSensitive();
}

void pasteFromClip(GtkWidget* widget,gpointer data)
{
	pageStruct*	page=getPageStructPtr(-1);
	gtk_text_buffer_paste_clipboard((GtkTextBuffer*)page->buffer,gtk_clipboard_get(GDK_SELECTION_CLIPBOARD),NULL,true);
	setSensitive();
}

void undo(GtkWidget* widget,gpointer data)
{
	pageStruct*	page=getPageStructPtr(-1);

	if(gtk_source_buffer_can_undo(page->buffer));
		{
			gtk_source_buffer_undo(page->buffer);
			page->isFirst=true;
			setSensitive();
		}
}

void redo(GtkWidget* widget,gpointer data)
{
	pageStruct*	page=getPageStructPtr(-1);

	if(gtk_source_buffer_can_redo(page->buffer));
		{
			gtk_source_buffer_redo(page->buffer);
			page->isFirst=true;
			setSensitive();
		}
}



void openHelp(GtkWidget* widget,gpointer data)
{
	char*	runhelp;
	asprintf(&runhelp,"xdg-open %s/help/help.html",DATADIR);
	system(runhelp);
}

void copyToClipboard(GtkWidget* widget,gpointer data)
{
	GtkClipboard*	clipboard=gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_set_text(clipboard,(char*)data,-1);
}

void populatePopupMenu(GtkTextView *entry,GtkMenu *menu,gpointer user_data)
{
	pageStruct*		page=getPageStructPtr(-1);
	GtkTextIter		start;
	GtkTextIter		end;
	char*			selection=NULL;
	GtkWidget*		menuitem;

	menuitem=gtk_separator_menu_item_new();
	gtk_menu_shell_prepend(GTK_MENU_SHELL(menu),menuitem);

	if(gtk_text_buffer_get_selection_bounds((GtkTextBuffer*)page->buffer,&start,&end))
		{
			selection=gtk_text_buffer_get_text((GtkTextBuffer*)page->buffer,&start,&end,false);
			if(selection!=NULL)
				{
#ifdef _ASPELL_
//spell check
					menuitem=gtk_image_menu_item_new_with_label("Check Spellling");
					image=gtk_image_new_from_stock(GTK_STOCK_SPELL_CHECK,GTK_ICON_SIZE_MENU);
					gtk_image_menu_item_set_image((GtkImageMenuItem *)menuitem,image);
					gtk_menu_shell_prepend(GTK_MENU_SHELL(menu),menuitem);
					gtk_signal_connect(GTK_OBJECT(menuitem),"activate",G_CALLBACK(checkWord),NULL);
#endif
					menuitem=gtk_separator_menu_item_new();
					gtk_menu_shell_append(GTK_MENU_SHELL(menu),menuitem);
				}
		}
	gtk_widget_show_all((GtkWidget*)menu);
}

void doTabMenu(GtkWidget *widget,gpointer user_data)
{
	GtkClipboard*	clipboard=gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);

	gtk_clipboard_set_text(clipboard,(char*)user_data,-1);
	gtk_widget_destroy(tabMenu);
}

gboolean whatPane(GtkWidget *widget,GdkEvent *event,gpointer data)
{
	pageStruct* page=(pageStruct*)getPageStructPtr(-1);

	if((long)data==1)
		page->inTop=true;
	else
		page->inTop=false;

	return(false);
}

void doSplitView(GtkWidget *widget,gpointer user_data)
{
	pageStruct* page=(pageStruct*)user_data;

	if(gtk_paned_get_child2((GtkPaned*)page->pane)==NULL)
		{
			page->pageWindow2=(GtkScrolledWindow*)gtk_scrolled_window_new(NULL, NULL);
			gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(page->pageWindow2),GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

			page->view2 = (GtkSourceView*)gtk_source_view_new_with_buffer (page->buffer);
			g_signal_connect(G_OBJECT(page->view2),"populate-popup",G_CALLBACK(populatePopupMenu),NULL);
			setFilePrefs((GtkSourceView*)page->view2);

			gtk_paned_add2(GTK_PANED(page->pane),(GtkWidget*)page->pageWindow2);
			gtk_container_add(GTK_CONTAINER((GtkWidget*)page->pageWindow2),(GtkWidget*)page->view2);
			g_signal_connect(G_OBJECT(page->view2),"button-release-event",G_CALLBACK(whatPane),(void*)2);
			page->isSplit=true;
		}
	else
		{
			page->isSplit=false;
			gtk_widget_destroy(gtk_paned_get_child2((GtkPaned*)page->pane));
		}

	gtk_widget_show_all(page->pane);
}

bool tabPopUp(GtkWidget *widget, GdkEventButton *event,gpointer user_data)
{
	pageStruct* page;
	GtkWidget*	menuitem;
	GtkWidget*	image;

	if(event->button==3 && event->type==GDK_BUTTON_PRESS)
	    {
			tabMenu=gtk_menu_new();
			page=(pageStruct*)user_data;

//copy filepath
			image=gtk_image_new_from_stock(GTK_STOCK_COPY,GTK_ICON_SIZE_MENU);
			menuitem=gtk_image_menu_item_new_with_label("Copy Filepath");
			gtk_image_menu_item_set_image((GtkImageMenuItem*)menuitem,image);
			gtk_menu_shell_append(GTK_MENU_SHELL(tabMenu),menuitem);
			gtk_signal_connect(GTK_OBJECT(menuitem),"activate",G_CALLBACK(doTabMenu),(void*)page->filePath);
//copy filename
			image=gtk_image_new_from_stock(GTK_STOCK_COPY,GTK_ICON_SIZE_MENU);
			menuitem=gtk_image_menu_item_new_with_label("Copy FileName");
			gtk_image_menu_item_set_image((GtkImageMenuItem*)menuitem,image);
			gtk_menu_shell_append(GTK_MENU_SHELL(tabMenu),menuitem);
			gtk_signal_connect(GTK_OBJECT(menuitem),"activate",G_CALLBACK(doTabMenu),(void*)page->fileName);

#ifdef _ASPELL_
//check document
			image=gtk_image_new_from_stock(GTK_STOCK_SPELL_CHECK,GTK_ICON_SIZE_MENU);
			menuitem=gtk_image_menu_item_new_with_label("Spell Check Document");
			gtk_image_menu_item_set_image((GtkImageMenuItem*)menuitem,image);
			gtk_menu_shell_append(GTK_MENU_SHELL(tabMenu),menuitem);
			gtk_signal_connect(GTK_OBJECT(menuitem),"activate",G_CALLBACK(doSpellCheckDoc),(void*)page->filePath);
#endif

//paned view
			image=gtk_image_new_from_stock(GTK_STOCK_NEW,GTK_ICON_SIZE_MENU);
			if(page->isSplit==true)
				menuitem=gtk_image_menu_item_new_with_label("Un-Split View");
			else
				menuitem=gtk_image_menu_item_new_with_label("Split View");

			gtk_image_menu_item_set_image((GtkImageMenuItem*)menuitem,image);
			gtk_menu_shell_append(GTK_MENU_SHELL(tabMenu),menuitem);
			gtk_signal_connect(GTK_OBJECT(menuitem),"activate",G_CALLBACK(doSplitView),(void*)page);

			gtk_menu_attach_to_widget(GTK_MENU(tabMenu),widget,NULL);
			gtk_menu_popup(GTK_MENU(tabMenu),NULL,NULL,NULL,NULL,event->button,event->time);
			gtk_widget_show_all((GtkWidget*)tabMenu);

			return(true);
		}
	else
		return(false);
}


void writeExitData(void)
{
	GtkAllocation	alloc;
	FILE*			fd=NULL;
	char*			filename;
	int				winx;
	int				winy;

	gtk_widget_get_allocation(window,&alloc);
	gtk_window_get_position((GtkWindow*)window,&winx,&winy);

	asprintf(&filename,"%s/.ManPageEditor",getenv("HOME"));
	g_mkdir_with_parents(filename,493);
	g_free(filename);
	asprintf(&filename,"%s/.ManPageEditor/editor.window.rc",getenv("HOME"));
	fd=fopen(filename,"w");
	if(fd!=NULL)
		{
			fprintf(fd,"windowsize	%i %i %i %i\n",alloc.width,alloc.height,winx,winy);
			fclose(fd);
		}
	g_free(filename);
}

void writeConfig(void)
{
	GtkAllocation	alloc;
	FILE*			fd=NULL;
	char*			filename;
	int				winx;
	int				winy;

	gtk_widget_get_allocation(window,&alloc);
	gtk_window_get_position((GtkWindow*)window,&winx,&winy);

	asprintf(&filename,"%s/.ManPageEditor",getenv("HOME"));
	g_mkdir_with_parents(filename,493);
	g_free(filename);
	asprintf(&filename,"%s/.ManPageEditor/editor.rc",getenv("HOME"));
	fd=fopen(filename,"w");
	if(fd!=NULL)
		{


			fprintf(fd,"wrapline	%i\n",(int)lineWrap);
			fprintf(fd,"highlightcurrentline	%i\n",(int)highLight);

			fprintf(fd,"insenssearch	%i\n",(int)insensitiveSearch);
			fprintf(fd,"wrapsearch	%i\n",(int)wrapSearch);


			fprintf(fd,"showjtoline	%i\n",(int)showJumpToLine);


			fprintf(fd,"showlivesearch	%i\n",(int)showLiveSearch);

			fprintf(fd,"tabwidth	%i\n",tabWidth);

			fprintf(fd,"font	%s\n",fontAndSize);

			fclose(fd);
		}
	g_free(filename);
}

bool doSaveAll(GtkWidget* widget,gpointer data)
{
	int			numpages=gtk_notebook_get_n_pages(notebook);
	int			result;
	pageStruct*	page;

	for(int loop=0;loop<numpages;loop++)
		{
			page=getPageStructPtr(loop);
			if(gtk_text_buffer_get_modified(GTK_TEXT_BUFFER(page->buffer)))
				{
					if((bool)data==true)
						{
							result=show_question(g_path_get_basename(page->fileName));
							switch(result)
								{
									case GTK_RESPONSE_YES:
										gtk_notebook_set_current_page(notebook,loop);
										saveFile(NULL,NULL);
										
										break;
									case GTK_RESPONSE_NO:
										break;
									default:
										return(false);;
										break;
								}
						}
					else
						{
							gtk_notebook_set_current_page(notebook,loop);
							saveFile(NULL,NULL);
						}
				}
		}
	return(true);
}

void doShutdown(GtkWidget* widget,gpointer data)
{
	char*	command;

	asprintf(&command,"rm %s &>/dev/null",htmlFile);
	if(doSaveAll(widget,(void*)true)==true)
		{
			writeExitData();
			gtk_main_quit();
		}
	system(command);
	g_free(command);

#ifdef _ASPELL_
	delete_aspell_config(aspellConfig);
	delete_aspell_speller(spellChecker);
#endif
}

void setPrefs(GtkWidget* widget,gpointer data)
{
	if(strcmp(gtk_widget_get_name(widget),"wrap")==0)
		tmpLineWrap=gtk_toggle_button_get_active((GtkToggleButton*)data);
	if(strcmp(gtk_widget_get_name(widget),"high")==0)
		tmpHighLight=gtk_toggle_button_get_active((GtkToggleButton*)data);

	if(strcmp(gtk_widget_get_name(widget),"jtolintool")==0)
		tmpShowJumpToLine=gtk_toggle_button_get_active((GtkToggleButton*)data);
	if(strcmp(gtk_widget_get_name(widget),"livesearch")==0)
		tmpShowLiveSearch=gtk_toggle_button_get_active((GtkToggleButton*)data);

	if(strcmp(gtk_widget_get_name(widget),"tabs")==0)
		tmpTabWidth=(int)gtk_spin_button_get_value((GtkSpinButton*)data);

	if(strcmp(gtk_widget_get_name(widget),"cancel")==0)
		gtk_widget_destroy(prefswin);

	if(strcmp(gtk_widget_get_name(widget),"apply")==0)
		{
			lineWrap=tmpLineWrap;
			highLight=tmpHighLight;
			showJumpToLine=tmpShowJumpToLine;
			showLiveSearch=tmpShowLiveSearch;
			showHideWidget(lineNumberWidget,showJumpToLine);
			showHideWidget(liveSearchWidget,showLiveSearch);

			if(fontAndSize!=NULL)
				{
					g_free(fontAndSize);
					fontAndSize=strdup(gtk_entry_get_text((GtkEntry*)fontBox));
				}

			tabWidth=tmpTabWidth;
			gtk_widget_destroy(prefswin);
			resetAllFilePrefs();
			writeConfig();
		}
}

void doAbout(GtkWidget* widget,gpointer data)
{
	const char*	authors[]={"K.D.Hedger <"MYEMAIL">",NULL};
	const char	copyright[] ="Copyright \xc2\xa9 2013 K.D.Hedger";
	const char*	aboutboxstring="ManPageEditor Code Text Editor";

	gtk_show_about_dialog(NULL,"authors",authors,"comments",aboutboxstring,"copyright",copyright,"version",KKEDIT_VERSION,"website",MYWEBSITE,"program-name","ManPageEditor","logo-icon-name","ManPageEditor",NULL); 
}

void drawPage(GtkPrintOperation *operation,GtkPrintContext *context,gint page_nr,gpointer user_data)
{
	GtkSourcePrintCompositor *compositor;

	compositor=GTK_SOURCE_PRINT_COMPOSITOR(user_data);
	gtk_source_print_compositor_draw_page(compositor,context,page_nr);
}

void beginPrint(GtkPrintOperation *operation,GtkPrintContext *context,gpointer user_data)
{
	GtkSourcePrintCompositor*	compositor;
	gint						n_pages;

    compositor=GTK_SOURCE_PRINT_COMPOSITOR (user_data);

    while(!gtk_source_print_compositor_paginate(compositor,context));

    n_pages=gtk_source_print_compositor_get_n_pages(compositor);
    gtk_print_operation_set_n_pages(operation,n_pages);
}

void printFile(GtkWidget* widget,gpointer data)
{
	pageStruct*					page=getPageStructPtr(-1);
	GtkSourcePrintCompositor*	printview=gtk_source_print_compositor_new_from_view(page->view);
	GtkPrintOperation*			print;
	GtkPrintOperationResult		result;

	print=gtk_print_operation_new ();
	if (settings != NULL)
		gtk_print_operation_set_print_settings(print,settings);
	g_signal_connect(print,"begin-print",G_CALLBACK(beginPrint),(void*)printview);
	g_signal_connect(print,"draw-page",G_CALLBACK (drawPage),(void*)printview);
	result=gtk_print_operation_run(print,GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,GTK_WINDOW(window),NULL);
	if (result==GTK_PRINT_OPERATION_RESULT_APPLY)
		{
			if (settings != NULL)
				g_object_unref(settings);
			settings=(GtkPrintSettings*)g_object_ref((gpointer)gtk_print_operation_get_print_settings(print));
		}
	g_object_unref(print);
	g_object_unref(printview);
}

void newEditor(GtkWidget* widget,gpointer data)
{
	system("manpageeditor");
}

bool bold=false;
GtkTextTag *boldtag;
int	boldnum=0;
int	breaknum=0;

void doFormat(GtkWidget* widget,gpointer data)
{
	pageStruct*	page=getPageStructPtr(-1);
	GtkTextMark*	mark;
	GtkTextIter		iter;
	GtkTextIter		start;
	GtkTextIter		end;
	char*			name;

	mark=gtk_text_buffer_get_insert((GtkTextBuffer*)page->buffer);
	gtk_text_buffer_get_iter_at_mark((GtkTextBuffer*)page->buffer,&iter,mark);

	switch((long)data)
		{
			case 1:
					asprintf(&name,"bold-%i",boldnum);
					boldtag=gtk_text_buffer_create_tag((GtkTextBuffer*)page->buffer,name,"weight",PANGO_WEIGHT_BOLD,NULL);
					gtk_text_buffer_get_selection_bounds((GtkTextBuffer*)page->buffer,&start,&end);
					gtk_text_buffer_apply_tag((GtkTextBuffer*)page->buffer,boldtag,&start,&end);
					bold=true;
					boldnum++;
					tagList[currentTagNum]=(tagStruct*)malloc(sizeof(tagStruct));
					tagList[currentTagNum]->name=strdup(name);
					tagList[currentTagNum]->startLine=gtk_text_iter_get_line(&start);
					tagList[currentTagNum]->startOffset=gtk_text_iter_get_line_offset(&start);
					tagList[currentTagNum]->endLine=gtk_text_iter_get_line(&end);
					tagList[currentTagNum]->endOffset=gtk_text_iter_get_line_offset(&end);
					tagList[currentTagNum]->isTag=true;
					currentTagNum++;
					g_free(name);
				break;
			case 2:
//				asprintf(&name,"BOLDOFF",marknum);
				bold=false;
				break;

			case 3:
				asprintf(&name,"break-%i",breaknum);
				breaknum++;
				tagList[currentTagNum]=(tagStruct*)malloc(sizeof(tagStruct));
				tagList[currentTagNum]->name=strdup(name);
				tagList[currentTagNum]->startLine=gtk_text_iter_get_line(&iter);
				tagList[currentTagNum]->startOffset=0;
				tagList[currentTagNum]->isTag=false;
				gtk_text_iter_set_offset(&iter,0);
				tagList[currentTagNum]->mark=gtk_text_buffer_create_mark((GtkTextBuffer*)page->buffer,name,&iter,true);
				gtk_text_mark_set_visible(tagList[currentTagNum]->mark,true);
				currentTagNum++;
				g_free(name);
				break;
		}
		
//	gtk_text_buffer_create_mark((GtkTextBuffer*)page->buffer,name,&iter,true);
//	printf("thing=%i\n",(int)(long)data);
//	printf("start line - %i\nchar -%i\n",gtk_text_iter_get_line(&start),gtk_text_iter_get_line_offset(&start));
//	printf("end line - %i\nchar -%i\n",gtk_text_iter_get_line(&end),gtk_text_iter_get_line_offset(&end));
	
	refreshMainWindow();
}














