//
// C++ Implementation: MainWindow
//
// Description:
//
//
// Author: Jally <jallyx@163.com>, (C) 2008
//
// Copyright: See COPYING file that comes with this distribution
//
//
#include "MainWindow.h"

#include <string>
#include <inttypes.h>

#include "iptux/Command.h"
#include "iptux/DataSettings.h"
#include "iptux/DetectPal.h"
#include "iptux/DialogGroup.h"
#include "iptux/DialogPeer.h"
#include "iptux/HelpDialog.h"
#include "iptux/RevisePal.h"
#include "iptux/ShareFile.h"
#include "iptux/StatusIcon.h"
#include "iptux/callback.h"
#include "iptux/deplib.h"
#include "iptux/global.h"
#include "iptux/support.h"
#include "iptux/utils.h"
#include "iptux/output.h"
#include "iptux/TransWindow.h"

using namespace std;

namespace iptux {

static const int TRANS_TREE_MAX = 14;

/**
 * 类构造函数.
 */
MainWindow::MainWindow(GtkApplication* app, IptuxConfig &config, ProgramData &progdt)
    : app(app),
      window(nullptr),
      config(config),
      progdt(progdt),
      widset(NULL),
      mdlset(NULL),
      tmdllist(NULL),
      accel(NULL),
      timerid(0),
      windowConfig(250, 510, "main_window") {
  activeWindowType = ActiveWindowType ::OTHERS;
  activeWindow = nullptr;
  transWindow = nullptr;
  windowConfig.LoadFromConfig(config);
}

/**
 * 类析构函数.
 */
MainWindow::~MainWindow() {
  if(transWindow) {
    g_object_unref(transWindow);
  }
  ClearSublayer();
}

GtkWidget* MainWindow::getWindow() {
  return window;
}

typedef void (* GActionCallback) (GSimpleAction *action,
                                  GVariant      *parameter,
                                  gpointer       user_data) ;
#define	G_ACTION_CALLBACK(f)			 ((GActionCallback) (f))

/**
 * 创建程序主窗口入口.
 */
void MainWindow::CreateWindow() {
  GtkWidget *widget;

  InitSublayer();

  /* 创建主窗口 */
  window = CreateMainWindow();
  g_object_set_data(G_OBJECT(window), "iptux-config", &config);
  g_object_set_data_full(G_OBJECT(window), "trans-model", CreateTransModel(),
                           GDestroyNotify(g_object_unref));

  gtk_container_add(GTK_CONTAINER(window), CreateAllArea());
  gtk_widget_show_all(window);


  GActionEntry win_entries[] = {
      { "refresh", G_ACTION_CALLBACK(onRefresh)},
      { "sort_type", G_ACTION_CALLBACK(onSortType), "s" },
      { "sort_by", G_ACTION_CALLBACK(onSortBy), "s" },
      { "detect", G_ACTION_CALLBACK(onDetect)},
      { "find", G_ACTION_CALLBACK(onFind)},
      { "about", G_ACTION_CALLBACK(onAbout)},
      { "clear_chat_history", G_ACTION_CALLBACK(onClearChatHistory)},
      { "insert_picture", G_ACTION_CALLBACK(onInsertPicture)},
  };

  add_accelerator(app, "win.refresh", "F5");
  add_accelerator(app, "win.detect", "<Primary>D");
  add_accelerator(app, "win.find", "<Primary>F");

  g_action_map_add_action_entries (G_ACTION_MAP (window),
                                   win_entries, G_N_ELEMENTS (win_entries),
                                   this);

  /* 聚焦到好友树(paltree)区域 */
  widget = GTK_WIDGET(g_datalist_get_data(&widset, "paltree-treeview-widget"));
  gtk_widget_grab_focus(widget);
  /* 隐藏好友清单 */
  widget = GTK_WIDGET(g_datalist_get_data(&widset, "pallist-box-widget"));
  gtk_widget_hide(widget);

  if (progdt.IsAutoHidePanelAfterLogin()) {
    gtk_widget_hide(window);
  }
}

/**
 * 更改窗口显示模式.
 */
void MainWindow::AlterWindowMode() {
  GtkWidget *window;

  window = GTK_WIDGET(g_datalist_get_data(&widset, "window-widget"));
  if (gtk_widget_get_visible(window)) {
    gtk_window_iconify(GTK_WINDOW(window));
  } else {
    gtk_window_deiconify(GTK_WINDOW(window));
  }
}

/**
 * 获取主窗口指针.
 * @return 主窗口
 */
GtkWidget *MainWindow::ObtainWindow() {
  return GTK_WIDGET(g_datalist_get_data(&widset, "window-widget"));
}

/**
 * 好友树(paltree)中是否已经包含此IP地址的好友信息数据.
 * @param ipv4 ipv4
 * @return 是否包含
 */
bool MainWindow::PaltreeContainItem(in_addr_t ipv4) {
  GtkTreeModel *model;
  GtkTreeIter iter;
  GroupInfo *grpinf;
  PalInfo *pal;
  bool exist;

  if (!(pal = g_cthrd->GetPalFromList(ipv4)) ||
      !(grpinf = g_cthrd->GetPalRegularItem(pal)))
    return false;
  model = GTK_TREE_MODEL(g_datalist_get_data(&mdlset, "regular-paltree-model"));
  exist = GroupGetPaltreeItem(model, &iter, grpinf);

  return exist;
}

/**
 * 更新此IP地址好友在好友树(paltree)中的信息数据.
 * @param ipv4 ipv4
 */
void MainWindow::UpdateItemToPaltree(in_addr_t ipv4) {
  GtkTreeModel *model;
  GtkTreeIter parent, iter;
  GroupInfo *pgrpinf, *grpinf;
  PalInfo *pal;

  if (!(pal = g_cthrd->GetPalFromList(ipv4)) ||
      !(grpinf = g_cthrd->GetPalRegularItem(pal)))
    return;

  /* 更新常规模式树 */
  model = GTK_TREE_MODEL(g_datalist_get_data(&mdlset, "regular-paltree-model"));
  GroupGetPaltreeItem(model, &iter, grpinf);
  UpdateGroupInfoToPaltree(model, &iter, grpinf);
  /* 更新网段模式树 */
  model = GTK_TREE_MODEL(g_datalist_get_data(&mdlset, "segment-paltree-model"));
  pgrpinf = g_cthrd->GetPalSegmentItem(pal);
  GroupGetPaltreeItem(model, &iter, pgrpinf);
  GroupGetPaltreeItemWithParent(model, &iter, grpinf);
  UpdateGroupInfoToPaltree(model, &iter, grpinf);
  /* 更新分组模式树 */
  model = GTK_TREE_MODEL(g_datalist_get_data(&mdlset, "group-paltree-model"));
  pgrpinf = g_cthrd->GetPalGroupItem(pal);
  GroupGetPrevPaltreeItem(model, &iter, grpinf);
  gtk_tree_model_iter_parent(model, &parent, &iter);
  if (gtk_tree_model_iter_n_children(model, &parent) == 1)
    gtk_tree_store_remove(GTK_TREE_STORE(model), &parent);
  else
    gtk_tree_store_remove(GTK_TREE_STORE(model), &iter);
  if (!GroupGetPaltreeItem(model, &parent, pgrpinf)) {
    gtk_tree_store_append(GTK_TREE_STORE(model), &parent, NULL);
    FillGroupInfoToPaltree(model, &parent, pgrpinf);
  }
  gtk_tree_store_append(GTK_TREE_STORE(model), &iter, &parent);
  FillGroupInfoToPaltree(model, &iter, grpinf);
  UpdateGroupInfoToPaltree(model, &parent, pgrpinf);
  /* 更新广播模式树 */
  model =
      GTK_TREE_MODEL(g_datalist_get_data(&mdlset, "broadcast-paltree-model"));
  pgrpinf = g_cthrd->GetPalBroadcastItem(pal);
  GroupGetPaltreeItem(model, &iter, pgrpinf);
  GroupGetPaltreeItemWithParent(model, &iter, grpinf);
  UpdateGroupInfoToPaltree(model, &iter, grpinf);
}

/**
 * 附加此IP地址的好友到好友树(paltree).
 * @param ipv4 ipv4
 */
void MainWindow::AttachItemToPaltree(in_addr_t ipv4) {
  GtkTreeModel *model;
  GtkTreeIter parent, iter;
  GroupInfo *pgrpinf, *grpinf;
  PalInfo *pal;

  if (!(pal = g_cthrd->GetPalFromList(ipv4)) ||
      !(grpinf = g_cthrd->GetPalRegularItem(pal)))
    return;

  /* 添加到常规模式树 */
  model = GTK_TREE_MODEL(g_datalist_get_data(&mdlset, "regular-paltree-model"));
  gtk_tree_store_append(GTK_TREE_STORE(model), &iter, NULL);
  FillGroupInfoToPaltree(model, &iter, grpinf);
  /* 添加到网段模式树 */
  model = GTK_TREE_MODEL(g_datalist_get_data(&mdlset, "segment-paltree-model"));
  pgrpinf = g_cthrd->GetPalSegmentItem(pal);
  if (!GroupGetPaltreeItem(model, &parent, pgrpinf)) {
    gtk_tree_store_append(GTK_TREE_STORE(model), &parent, NULL);
    FillGroupInfoToPaltree(model, &parent, pgrpinf);
  }
  gtk_tree_store_append(GTK_TREE_STORE(model), &iter, &parent);
  FillGroupInfoToPaltree(model, &iter, grpinf);
  UpdateGroupInfoToPaltree(model, &parent, pgrpinf);
  /* 添加到分组模式树 */
  model = GTK_TREE_MODEL(g_datalist_get_data(&mdlset, "group-paltree-model"));
  pgrpinf = g_cthrd->GetPalGroupItem(pal);
  if (!GroupGetPaltreeItem(model, &parent, pgrpinf)) {
    gtk_tree_store_append(GTK_TREE_STORE(model), &parent, NULL);
    FillGroupInfoToPaltree(model, &parent, pgrpinf);
  }
  gtk_tree_store_append(GTK_TREE_STORE(model), &iter, &parent);
  FillGroupInfoToPaltree(model, &iter, grpinf);
  UpdateGroupInfoToPaltree(model, &parent, pgrpinf);
  /* 添加到广播模式树 */
  model =
      GTK_TREE_MODEL(g_datalist_get_data(&mdlset, "broadcast-paltree-model"));
  pgrpinf = g_cthrd->GetPalBroadcastItem(pal);
  if (!GroupGetPaltreeItem(model, &parent, pgrpinf)) {
    gtk_tree_store_append(GTK_TREE_STORE(model), &parent, NULL);
    FillGroupInfoToPaltree(model, &parent, pgrpinf);
  }
  gtk_tree_store_append(GTK_TREE_STORE(model), &iter, &parent);
  FillGroupInfoToPaltree(model, &iter, grpinf);
  UpdateGroupInfoToPaltree(model, &parent, pgrpinf);
}

/**
 * 从好友树(paltree)中删除此IP地址的好友.
 * @param ipv4 ipv4
 */
void MainWindow::DelItemFromPaltree(in_addr_t ipv4) {
  GtkTreeModel *model;
  GtkTreeIter parent, iter;
  GroupInfo *pgrpinf, *grpinf;
  PalInfo *pal;

  if (!(pal = g_cthrd->GetPalFromList(ipv4)) ||
      !(grpinf = g_cthrd->GetPalRegularItem(pal)))
    return;

  /* 从常规模式树移除 */
  model = GTK_TREE_MODEL(g_datalist_get_data(&mdlset, "regular-paltree-model"));
  GroupGetPaltreeItem(model, &iter, grpinf);
  gtk_tree_store_remove(GTK_TREE_STORE(model), &iter);
  /* 从网段模式树移除 */
  model = GTK_TREE_MODEL(g_datalist_get_data(&mdlset, "segment-paltree-model"));
  pgrpinf = g_cthrd->GetPalSegmentItem(pal);
  GroupGetPaltreeItem(model, &parent, pgrpinf);
  if (g_slist_length(pgrpinf->member) != 1) {
    iter = parent;
    GroupGetPaltreeItemWithParent(model, &iter, grpinf);
    gtk_tree_store_remove(GTK_TREE_STORE(model), &iter);
    UpdateGroupInfoToPaltree(model, &parent, pgrpinf);
  } else
    gtk_tree_store_remove(GTK_TREE_STORE(model), &parent);
  /* 从分组模式树移除 */
  model = GTK_TREE_MODEL(g_datalist_get_data(&mdlset, "group-paltree-model"));
  pgrpinf = g_cthrd->GetPalGroupItem(pal);
  GroupGetPaltreeItem(model, &parent, pgrpinf);
  if (g_slist_length(pgrpinf->member) != 1) {
    iter = parent;
    GroupGetPaltreeItemWithParent(model, &iter, grpinf);
    gtk_tree_store_remove(GTK_TREE_STORE(model), &iter);
    UpdateGroupInfoToPaltree(model, &parent, pgrpinf);
  } else
    gtk_tree_store_remove(GTK_TREE_STORE(model), &parent);
  /* 从广播模式树移除 */
  model =
      GTK_TREE_MODEL(g_datalist_get_data(&mdlset, "broadcast-paltree-model"));
  pgrpinf = g_cthrd->GetPalBroadcastItem(pal);
  GroupGetPaltreeItem(model, &parent, pgrpinf);
  if (g_slist_length(pgrpinf->member) != 1) {
    iter = parent;
    GroupGetPaltreeItemWithParent(model, &iter, grpinf);
    gtk_tree_store_remove(GTK_TREE_STORE(model), &iter);
    UpdateGroupInfoToPaltree(model, &parent, pgrpinf);
  } else
    gtk_tree_store_remove(GTK_TREE_STORE(model), &parent);
}

/**
 * 从好友树(paltree)中删除所有好友数据.
 */
void MainWindow::ClearAllItemFromPaltree() {
  GtkTreeModel *model;

  model = GTK_TREE_MODEL(g_datalist_get_data(&mdlset, "regular-paltree-model"));
  gtk_tree_store_clear(GTK_TREE_STORE(model));
  model = GTK_TREE_MODEL(g_datalist_get_data(&mdlset, "segment-paltree-model"));
  gtk_tree_store_clear(GTK_TREE_STORE(model));
  model = GTK_TREE_MODEL(g_datalist_get_data(&mdlset, "group-paltree-model"));
  gtk_tree_store_clear(GTK_TREE_STORE(model));
  model =
      GTK_TREE_MODEL(g_datalist_get_data(&mdlset, "broadcast-paltree-model"));
  gtk_tree_store_clear(GTK_TREE_STORE(model));
}

/**
 * 让指定的项在好友树(paltree)中闪烁.
 * @param grpinf class GroupInfo
 * @param blinking 是否继续闪烁
 * @note 如果我猜得没错，调用此函数的环境一定已经对(grpinf)加锁了
 */
void MainWindow::MakeItemBlinking(GroupInfo *grpinf, bool blinking) {
  GtkTreeModel *model;
  GtkTreeIter iter;
  GroupInfo *pgrpinf;

  /* 成员为空表明此项已经不存在model中，也就没必要再处理它了 */
  if (!grpinf->member) return;

  /* 闪烁项 */
  switch (grpinf->type) {
    case GROUP_BELONG_TYPE_REGULAR:
      /* 闪烁常规模式树 */
      model =
          GTK_TREE_MODEL(g_datalist_get_data(&mdlset, "regular-paltree-model"));
      GroupGetPaltreeItem(model, &iter, grpinf);
      BlinkGroupItemToPaltree(model, &iter, blinking);
      /* 闪烁网段模式树 */
      model =
          GTK_TREE_MODEL(g_datalist_get_data(&mdlset, "segment-paltree-model"));
      pgrpinf = g_cthrd->GetPalSegmentItem((PalInfo *)grpinf->member->data);
      GroupGetPaltreeItem(model, &iter, pgrpinf);
      GroupGetPaltreeItemWithParent(model, &iter, grpinf);
      BlinkGroupItemToPaltree(model, &iter, blinking);
      /* 闪烁分组模式树 */
      model =
          GTK_TREE_MODEL(g_datalist_get_data(&mdlset, "group-paltree-model"));
      pgrpinf = g_cthrd->GetPalGroupItem((PalInfo *)grpinf->member->data);
      GroupGetPaltreeItem(model, &iter, pgrpinf);
      GroupGetPaltreeItemWithParent(model, &iter, grpinf);
      BlinkGroupItemToPaltree(model, &iter, blinking);
      /* 闪烁广播模式树 */
      model = GTK_TREE_MODEL(
          g_datalist_get_data(&mdlset, "broadcast-paltree-model"));
      pgrpinf = g_cthrd->GetPalBroadcastItem((PalInfo *)grpinf->member->data);
      GroupGetPaltreeItem(model, &iter, pgrpinf);
      GroupGetPaltreeItemWithParent(model, &iter, grpinf);
      BlinkGroupItemToPaltree(model, &iter, blinking);
      break;
    case GROUP_BELONG_TYPE_SEGMENT:
      /* 闪烁网段模式树 */
      model =
          GTK_TREE_MODEL(g_datalist_get_data(&mdlset, "segment-paltree-model"));
      GroupGetPaltreeItem(model, &iter, grpinf);
      BlinkGroupItemToPaltree(model, &iter, blinking);
      break;
    case GROUP_BELONG_TYPE_GROUP:
      /* 闪烁分组模式树 */
      model =
          GTK_TREE_MODEL(g_datalist_get_data(&mdlset, "group-paltree-model"));
      GroupGetPaltreeItem(model, &iter, grpinf);
      BlinkGroupItemToPaltree(model, &iter, blinking);
      break;
    case GROUP_BELONG_TYPE_BROADCAST:
      /* 闪烁广播模式树 */
      model = GTK_TREE_MODEL(
          g_datalist_get_data(&mdlset, "broadcast-paltree-model"));
      GroupGetPaltreeItem(model, &iter, grpinf);
      BlinkGroupItemToPaltree(model, &iter, blinking);
      break;
    default:
      break;
  }
}

/**
 * 打开文件传输窗口.
 */
void MainWindow::OpenTransWindow() {
  GtkWidget *widget;
  guint timerid;

  if(transWindow == nullptr) {
    transWindow = trans_window_new(GTK_WINDOW(window));
    gtk_widget_show_all(transWindow);
    gtk_widget_hide(transWindow);
  }
  gtk_widget_show(transWindow);
  gtk_window_present(GTK_WINDOW(transWindow));
  widget = GTK_WIDGET(g_object_get_data(G_OBJECT(transWindow), "trans-treeview-widget"));
  timerid = gdk_threads_add_timeout(200, GSourceFunc(UpdateTransUI), widget);
  g_object_set_data(G_OBJECT(widget), "update-timer-id",
                    GUINT_TO_POINTER(timerid));
}

/**
 * 更新文件传输树(trans-tree)的指定项.
 * @param para 项值
 * @note 若项不存在则须自动加入
 */
void MainWindow::UpdateItemToTransTree(GData **para) {
  GtkTreeModel *model;
  GtkTreeIter iter;
  gpointer data;
  /* 查询项所在位置，若不存在则自动加入 */
  data = NULL;
  model = GTK_TREE_MODEL(g_object_get_data(G_OBJECT(window), "trans-model"));
  if (gtk_tree_model_get_iter_first(model, &iter)) {
    do {
      gtk_tree_model_get(model, &iter, TRANS_TREE_MAX, &data, -1);
      if (para == data) break;
    } while (gtk_tree_model_iter_next(model, &iter));
  }
  if (para != data) {
    gtk_list_store_append(GTK_LIST_STORE(model), &iter);
    gtk_list_store_set(GTK_LIST_STORE(model), &iter, TRANS_TREE_MAX, para, -1);
  }

  /**
   * @note 鉴于参数值(*para)的原地址有可能会被重用， 所以当("data"==null)
   * 时应该清空参数指针值，以防止其他后来项误认此项为自己的大本营.
   */
  if (!g_datalist_get_data(para, "data"))
    gtk_list_store_set(GTK_LIST_STORE(model), &iter, TRANS_TREE_MAX, NULL, -1);

  /* 重设数据 */
  gtk_list_store_set(
      GTK_LIST_STORE(model), &iter, 0, g_datalist_get_data(para, "status"), 1,
      g_datalist_get_data(para, "task"), 2, g_datalist_get_data(para, "peer"),
      3, g_datalist_get_data(para, "ip"), 4,
      g_datalist_get_data(para, "filename"), 5,
      g_datalist_get_data(para, "filelength"), 6,
      g_datalist_get_data(para, "finishlength"), 7,
      GPOINTER_TO_INT(g_datalist_get_data(para, "progress")), 8,
      g_datalist_get_data(para, "pro-text"), 9,
      g_datalist_get_data(para, "cost"), 10,
      g_datalist_get_data(para, "remain"), 11,
      g_datalist_get_data(para, "rate"), 12,
      g_datalist_get_data(para, "filepath"), 13,
      g_datalist_get_data(para, "data"), -1);
}

/**
 * 查询此刻是否存在活动的文件传输.
 * @return 活动与否
 */
bool MainWindow::TransmissionActive() {
  GtkTreeModel *model;
  GtkTreeIter iter;
  gpointer data;

  data = NULL;
  model = GTK_TREE_MODEL(g_object_get_data(G_OBJECT(window), "trans-model"));
  if (gtk_tree_model_get_iter_first(model, &iter)) {
    do {
      gtk_tree_model_get(model, &iter, TRANS_TREE_MAX, &data, -1);
      if (data) break;
    } while (gtk_tree_model_iter_next(model, &iter));
  }

  return data;
}

/**
 * 初始化底层数据.
 */
void MainWindow::InitSublayer() {
  GtkTreeModel *model;

  g_datalist_init(&widset);
  g_datalist_init(&mdlset);

  accel = gtk_accel_group_new();
  timerid = gdk_threads_add_timeout(1000, GSourceFunc(UpdateUI), this);

  model = CreatePaltreeModel();
  g_datalist_set_data_full(&mdlset, "regular-paltree-model", model,
                           GDestroyNotify(g_object_unref));
  tmdllist = g_list_append(tmdllist, model);
  model = CreatePaltreeModel();
  g_datalist_set_data_full(&mdlset, "segment-paltree-model", model,
                           GDestroyNotify(g_object_unref));
  tmdllist = g_list_append(tmdllist, model);
  model = CreatePaltreeModel();
  g_datalist_set_data_full(&mdlset, "group-paltree-model", model,
                           GDestroyNotify(g_object_unref));
  tmdllist = g_list_append(tmdllist, model);
  model = CreatePaltreeModel();
  g_datalist_set_data_full(&mdlset, "broadcast-paltree-model", model,
                           GDestroyNotify(g_object_unref));
  tmdllist = g_list_append(tmdllist, model);
  model = CreatePallistModel();
  g_datalist_set_data_full(&mdlset, "pallist-model", model,
                           GDestroyNotify(g_object_unref));
}

/**
 * 清空底层数据.
 */
void MainWindow::ClearSublayer() {
  g_datalist_clear(&widset);
  g_datalist_clear(&mdlset);
  g_list_free(tmdllist);
  if (accel) g_object_unref(accel);
  if (timerid > 0) g_source_remove(timerid);
}

/**
 * 创建主窗口.
 * @return 主窗口
 */
GtkWidget *MainWindow::CreateMainWindow() {
  GdkGeometry geometry = {50,
                          200,
                          G_MAXINT,
                          G_MAXINT,
                          0,
                          0,
                          2,
                          5,
                          0.0,
                          0.0,
                          GDK_GRAVITY_NORTH_WEST};
  GdkWindowHints hints = GdkWindowHints(
      GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE | GDK_HINT_BASE_SIZE |
      /*GDK_HINT_RESIZE_INC |*/ GDK_HINT_WIN_GRAVITY | GDK_HINT_USER_POS |
      GDK_HINT_USER_SIZE);
  window = gtk_application_window_new(app);
  gtk_window_set_icon_name(GTK_WINDOW(window), "iptux");
  gtk_window_set_title(GTK_WINDOW(window), _("iptux"));
  gtk_window_set_default_size(GTK_WINDOW(window), windowConfig.GetWidth(),
                              windowConfig.GetHeight());
  gtk_window_set_geometry_hints(GTK_WINDOW(window), window, &geometry, hints);
  gtk_window_set_default_icon_name("iptux");
  gtk_window_add_accel_group(GTK_WINDOW(window), accel);

  g_datalist_set_data(&widset, "window-widget", window);
  g_signal_connect_swapped(window, "delete-event", G_CALLBACK(onDeleteEvent),
                           this);
  g_signal_connect(window, "configure-event", G_CALLBACK(MWinConfigureEvent),
                   this);
  g_signal_connect_swapped(window, "notify::is-active", G_CALLBACK(onActive), this);
  return window;
}


/**
 * 创建所有区域.
 * @return 主窗体
 */
GtkWidget *MainWindow::CreateAllArea() {
  GtkWidget *box, *paned;

  box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

  gtk_box_pack_start(GTK_BOX(box), CreateToolBar(), FALSE, FALSE, 0);

  paned = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
  g_object_set_data(G_OBJECT(paned), "position-name",
                    (gpointer) "mwin-main-paned-divide");
  gtk_paned_set_position(GTK_PANED(paned),
                         config.GetInt("mwin_main_paned_divide", 210));
  gtk_container_set_border_width(GTK_CONTAINER(paned), 4);
  gtk_box_pack_start(GTK_BOX(box), paned, TRUE, TRUE, 0);
  g_signal_connect(paned, "notify::position", G_CALLBACK(PanedDivideChanged),
                   this);
  gtk_paned_pack1(GTK_PANED(paned), CreatePaltreeArea(), TRUE, TRUE);
  gtk_paned_pack2(GTK_PANED(paned), CreatePallistArea(), FALSE, TRUE);

  return box;
}

/**
 * 创建工具条.
 * @return 工具条
 */
GtkWidget *MainWindow::CreateToolBar() {
  GtkWidget *toolbar;
  GtkToolItem *toolitem;
  GtkWidget *widget;

  toolbar = gtk_toolbar_new();
  g_object_set(toolbar, "icon-size", 1, NULL);
  gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);

  toolitem = gtk_tool_button_new(
    gtk_image_new_from_icon_name("go-previous-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR),
    "Go previous"
  );
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), toolitem, -1);
  g_signal_connect_swapped(toolitem, "clicked", G_CALLBACK(GoPrevTreeModel),
                           this);

  toolitem = gtk_tool_item_new();
  gtk_tool_item_set_expand(toolitem, TRUE);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), toolitem, -1);
  widget = gtk_label_new(_("Pals Online: 0"));
  gtk_container_add(GTK_CONTAINER(toolitem), widget);
  g_datalist_set_data(&widset, "online-label-widget", widget);

  toolitem = gtk_tool_button_new(
    gtk_image_new_from_icon_name("go-next-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR),
    "Go next"
  );
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), toolitem, -1);
  g_signal_connect_swapped(toolitem, "clicked", G_CALLBACK(GoNextTreeModel),
                           this);

  return toolbar;
}

/**
 * 创建好友树区域.
 * @return 主窗体
 */
GtkWidget *MainWindow::CreatePaltreeArea() {
  GtkWidget *sw;
  GtkWidget *widget;
  GtkTreeModel *model;

  sw = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC,
                                 GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw),
                                      GTK_SHADOW_ETCHED_IN);

  model = GTK_TREE_MODEL(g_datalist_get_data(&mdlset, "regular-paltree-model"));
  widget = CreatePaltreeTree(model);
  g_object_set_data(G_OBJECT(widget), "paltree-model", model);
  gtk_container_add(GTK_CONTAINER(sw), widget);
  g_datalist_set_data(&widset, "paltree-treeview-widget", widget);

  return sw;
}

/**
 * 创建好友清单区域.
 * @return 主窗体
 */
GtkWidget *MainWindow::CreatePallistArea() {
  GtkWidget *box, *hbox;
  GtkWidget *sw, *button, *widget;
  GtkTreeModel *model;

  box = gtk_vbox_new(FALSE, 0);
  g_datalist_set_data(&widset, "pallist-box-widget", box);

  /* 创建好友清单部分 */
  sw = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC,
                                 GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw),
                                      GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start(GTK_BOX(box), sw, TRUE, TRUE, 0);
  model = GTK_TREE_MODEL(g_datalist_get_data(&mdlset, "pallist-model"));
  widget = CreatePallistTree(model);
  gtk_container_add(GTK_CONTAINER(sw), widget);
  g_datalist_set_data(&widset, "pallist-treeview-widget", widget);

  /* 创建接受搜索输入部分 */
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 0);
  /*/* 关闭按钮 */
  button = gtk_button_new();
  widget = gtk_image_new_from_stock(GTK_STOCK_CLOSE, GTK_ICON_SIZE_BUTTON);
  gtk_button_set_image(GTK_BUTTON(button), widget);
  g_object_set(button, "relief", GTK_RELIEF_NONE, NULL);
  gtk_widget_set_size_request(button, -1, 1);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
  g_signal_connect_swapped(button, "clicked", G_CALLBACK(HidePallistArea),
                           &widset);
  /*/* 输入框 */
  widget = gtk_entry_new();
  gtk_entry_set_activates_default(GTK_ENTRY(widget), TRUE);
  gtk_entry_set_icon_from_stock(GTK_ENTRY(widget), GTK_ENTRY_ICON_SECONDARY,
                                GTK_STOCK_FIND);
  gtk_widget_add_events(widget, GDK_KEY_PRESS_MASK);
  g_object_set(widget, "has-tooltip", TRUE, NULL);
  gtk_box_pack_start(GTK_BOX(hbox), widget, TRUE, TRUE, 0);
  g_signal_connect(widget, "query-tooltip", G_CALLBACK(entry_query_tooltip),
                   _("Search Pals"));
  g_signal_connect(widget, "key-press-event", G_CALLBACK(ClearPallistEntry),
                   NULL);
  g_signal_connect(widget, "changed", G_CALLBACK(PallistEntryChanged), &widset);
  g_datalist_set_data(&widset, "pallist-entry-widget", widget);

  return box;
}

/**
 * 好友树(paltree)底层数据结构.
 * 7,0 closed-expander,1 open-expander,2 info.,3 extras,4 style,5 color,6 data
 * \n 关闭的展开器;打开的展开器;群组信息;扩展信息;字体风格;字体颜色;群组数据 \n
 * @return paltree-model
 */
GtkTreeModel *MainWindow::CreatePaltreeModel() {
  GtkTreeStore *model;

  model = gtk_tree_store_new(7, GDK_TYPE_PIXBUF, GDK_TYPE_PIXBUF, G_TYPE_STRING,
                             G_TYPE_STRING, PANGO_TYPE_ATTR_LIST,
                             GDK_TYPE_COLOR, G_TYPE_POINTER);
  gtk_tree_sortable_set_default_sort_func(
      GTK_TREE_SORTABLE(model),
      GtkTreeIterCompareFunc(PaltreeCompareByNameFunc), NULL, NULL);
  gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(model),
                                       GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
                                       GTK_SORT_ASCENDING);

  return GTK_TREE_MODEL(model);
}

/**
 * 好友清单(pallist)底层数据结构.
 * 7,0 icon,1 name,2 group,3 ipstr,4 user,5 host,6 data \n
 * 好友头像;好友昵称;好友群组;IP地址串;好友用户;好友主机;好友数据 \n
 * @return pallist-model
 * @note 鉴于好友清单(pallist)常年保持隐藏状态，所以请不要有事没事就往
 * 此model中填充数据，好友清单也无须与好友最新状态保持同步.
 */
GtkTreeModel *MainWindow::CreatePallistModel() {
  GtkListStore *model;

  model = gtk_list_store_new(7, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING,
                             G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                             G_TYPE_POINTER);

  return GTK_TREE_MODEL(model);
}

/**
 * 文件传输树(trans-tree)底层数据结构.
 * 14,0 status,1 task,2 peer,3 ip,4 filename,5 filelength,6 finishlength,7
 * progress, 8 pro-text,9 cost,10 remain,11 rate,12,pathname,13 data,14 para \n
 * 任务状态;任务类型;任务对端;文件名(如果当前是文件夹，该项指正在传输的文件夹内单个文件,
 * 整个文件夹传输完成后,该项指向当前是文件夹);文件长度;完成长度;完成进度;
 * 进度串;已花费时间;任务剩余时间;传输速度;带路径文件名(不显示);文件传输类;参数指针值
 * \n
 * @return trans-model
 */
GtkTreeModel *MainWindow::CreateTransModel() {
  GtkListStore *model;

  model = gtk_list_store_new(15, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING,
                             G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                             G_TYPE_STRING, G_TYPE_INT, G_TYPE_STRING,
                             G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                             G_TYPE_STRING, G_TYPE_POINTER, G_TYPE_POINTER);

  return GTK_TREE_MODEL(model);
}

/**
 * 创建好友树(paltree).
 * @param model paltree-model
 * @return 好友树
 */
GtkWidget *MainWindow::CreatePaltreeTree(GtkTreeModel *model) {
  GtkWidget *view;
  GtkTreeSelection *selection;
  GtkTreeViewColumn *column;
  GtkCellRenderer *cell;

  view = gtk_tree_view_new_with_model(model);
  gtk_tree_view_set_level_indentation(GTK_TREE_VIEW(view), 10);
  gtk_tree_view_set_show_expanders(GTK_TREE_VIEW(view), FALSE);
  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), FALSE);
  widget_enable_dnd_uri(view);
  g_object_set(view, "has-tooltip", TRUE, NULL);
  selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
  gtk_tree_selection_set_mode(GTK_TREE_SELECTION(selection),
                              GTK_SELECTION_NONE);

  column = gtk_tree_view_column_new();
  gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);
  g_object_set_data(G_OBJECT(view), "info-column", column);
  /* 展开器区域 */
  cell = gtk_cell_renderer_pixbuf_new();
  g_object_set(cell, "follow-state", TRUE, NULL);
  gtk_tree_view_column_pack_start(column, cell, FALSE);
  gtk_tree_view_column_set_attributes(GTK_TREE_VIEW_COLUMN(column), cell,
                                      "pixbuf", 0, "pixbuf-expander-closed", 0,
                                      "pixbuf-expander-open", 1, NULL);
  g_object_set_data(G_OBJECT(column), "expander-cell", cell);
  /* 群组信息区域 */
  cell = gtk_cell_renderer_text_new();
  g_object_set(cell, "xalign", 0.0, "wrap-mode", PANGO_WRAP_WORD, NULL);
  gtk_tree_view_column_pack_start(column, cell, FALSE);
  gtk_tree_view_column_set_attributes(GTK_TREE_VIEW_COLUMN(column), cell,
                                      "text", 2, "attributes", 4,
                                      "foreground-gdk", 5, NULL);
  /* 扩展信息区域 */
  cell = gtk_cell_renderer_text_new();
  g_object_set(cell, "xalign", 0.0, "wrap-mode", PANGO_WRAP_WORD, NULL);
  gtk_tree_view_column_pack_start(column, cell, FALSE);
  gtk_tree_view_column_set_attributes(GTK_TREE_VIEW_COLUMN(column), cell,
                                      "text", 3, "attributes", 4,
                                      "foreground-gdk", 5, NULL);

  /* 连接信号 */
  g_signal_connect(view, "query-tooltip", G_CALLBACK(PaltreeQueryTooltip),
                   this);
  g_signal_connect(view, "row-activated", G_CALLBACK(onPaltreeItemActivated),
                   this);
  g_signal_connect(view, "drag-data-received",
                   G_CALLBACK(PaltreeDragDataReceived), this);
  g_signal_connect(view, "button-press-event", G_CALLBACK(PaltreePopupMenu),
                   NULL);
  g_signal_connect(view, "button-release-event",
                   G_CALLBACK(PaltreeChangeStatus), NULL);

  return view;
}

/**
 * 创建好友清单(pallist).
 * @param model pallist-model
 * @return 好友清单
 */
GtkWidget *MainWindow::CreatePallistTree(GtkTreeModel *model) {
  GtkWidget *view;
  GtkTreeViewColumn *column;
  GtkCellRenderer *cell;
  GtkTreeSelection *selection;

  view = gtk_tree_view_new_with_model(model);
  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), TRUE);
  gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(view), TRUE);
  widget_enable_dnd_uri(view);
  selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
  gtk_tree_selection_set_mode(GTK_TREE_SELECTION(selection),
                              GTK_SELECTION_NONE);

  column = gtk_tree_view_column_new();
  gtk_tree_view_column_set_resizable(column, TRUE);
  gtk_tree_view_column_set_title(column, _("Nickname"));
  cell = gtk_cell_renderer_pixbuf_new();
  g_object_set(cell, "follow-state", TRUE, NULL);
  gtk_tree_view_column_pack_start(column, cell, FALSE);
  gtk_tree_view_column_set_attributes(column, cell, "pixbuf", 0, NULL);
  cell = gtk_cell_renderer_text_new();
  gtk_tree_view_column_pack_start(column, cell, FALSE);
  gtk_tree_view_column_set_attributes(column, cell, "text", 1, NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);

  cell = gtk_cell_renderer_text_new();
  column = gtk_tree_view_column_new_with_attributes(_("Group"), cell, "text", 2,
                                                    NULL);
  gtk_tree_view_column_set_resizable(column, TRUE);
  gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);

  cell = gtk_cell_renderer_text_new();
  column = gtk_tree_view_column_new_with_attributes(_("IPv4"), cell, "text", 3,
                                                    NULL);
  gtk_tree_view_column_set_resizable(column, TRUE);
  gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);

  cell = gtk_cell_renderer_text_new();
  column = gtk_tree_view_column_new_with_attributes(_("User"), cell, "text", 4,
                                                    NULL);
  gtk_tree_view_column_set_resizable(column, TRUE);
  gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);

  cell = gtk_cell_renderer_text_new();
  column = gtk_tree_view_column_new_with_attributes(_("Host"), cell, "text", 5,
                                                    NULL);
  gtk_tree_view_column_set_resizable(column, TRUE);
  gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);

  g_signal_connect(view, "row-activated", G_CALLBACK(PallistItemActivated),
                   this);
  g_signal_connect(view, "drag-data-received",
                   G_CALLBACK(PallistDragDataReceived), this);

  return view;
}

/**
 * 获取项(grpinf)在数据集(model)中的当前位置.
 * @param model model
 * @param iter 位置由此返回
 * @param grpinf class GroupInfo
 * @return 是否查找成功
 */
bool MainWindow::GroupGetPrevPaltreeItem(GtkTreeModel *model, GtkTreeIter *iter,
                                         GroupInfo *grpinf) {
  GroupInfo *pgrpinf;
  GtkTreeIter parent;

  if (!gtk_tree_model_get_iter_first(model, &parent)) return false;
  do {
    gtk_tree_model_get(model, &parent, 6, &pgrpinf, -1);
    if (pgrpinf->grpid == grpinf->grpid) {
      *iter = parent;
      break;
    }
    if (!gtk_tree_model_iter_children(model, iter, &parent)) continue;
    do {
      gtk_tree_model_get(model, iter, 6, &pgrpinf, -1);
      if (pgrpinf->grpid == grpinf->grpid) break;
    } while (gtk_tree_model_iter_next(model, iter));
    if (pgrpinf->grpid == grpinf->grpid) break;
  } while (gtk_tree_model_iter_next(model, &parent));

  return (pgrpinf->grpid == grpinf->grpid);
}

/**
 * 获取项(grpinf)在数据集(model)中的位置.
 * @param model model
 * @param iter 位置由此返回
 * @param grpinf class GroupInfo
 * @return 是否查找成功
 */
bool MainWindow::GroupGetPaltreeItem(GtkTreeModel *model, GtkTreeIter *iter,
                                     GroupInfo *grpinf) {
  GroupInfo *pgrpinf;

  if (!gtk_tree_model_get_iter_first(model, iter)) return false;
  do {
    gtk_tree_model_get(model, iter, 6, &pgrpinf, -1);
    if (pgrpinf->grpid == grpinf->grpid) break;
  } while (gtk_tree_model_iter_next(model, iter));

  return (pgrpinf->grpid == grpinf->grpid);
}

/**
 * 获取项(grpinf)在数据集(model)中的位置.
 * @param model model
 * @param iter 父节点位置/位置由此返回
 * @param grpinf class GroupInfo
 * @return 是否查找成功
 */
bool MainWindow::GroupGetPaltreeItemWithParent(GtkTreeModel *model,
                                               GtkTreeIter *iter,
                                               GroupInfo *grpinf) {
  GtkTreeIter parent;
  GroupInfo *pgrpinf;

  parent = *iter;
  if (!gtk_tree_model_iter_children(model, iter, &parent)) return false;
  do {
    gtk_tree_model_get(model, iter, 6, &pgrpinf, -1);
    if (pgrpinf->grpid == grpinf->grpid) break;
  } while (gtk_tree_model_iter_next(model, iter));

  return (pgrpinf->grpid == grpinf->grpid);
}

/**
 * 填充群组数据(grpinf)到数据集(model)指定位置(iter).
 * @param model model
 * @param iter iter
 * @param grpinf class GroupInfo
 */
void MainWindow::FillGroupInfoToPaltree(GtkTreeModel *model, GtkTreeIter *iter,
                                        GroupInfo *grpinf) {
  static GdkColor color = {0xff, 0x5252, 0xb8b8, 0x3838};
  GtkIconTheme *theme;
  GdkPixbuf *cpixbuf, *opixbuf= nullptr;
  PangoAttrList *attrs;
  PangoAttribute *attr;
  PangoFontDescription *dspt;
  char ipstr[INET_ADDRSTRLEN];
  gchar *file, *info, *extra;
  PalInfo *pal;
  GError* error = nullptr;

  /* 创建图标 */
  theme = gtk_icon_theme_get_default();
  if (grpinf->type == GROUP_BELONG_TYPE_REGULAR) {
    pal = (PalInfo *)grpinf->member->data;
    file = iptux_erase_filename_suffix(pal->iconfile);
    cpixbuf = gtk_icon_theme_load_icon(theme, file, MAX_ICONSIZE,
                                       GtkIconLookupFlags(0), &error);
    if(cpixbuf == nullptr) {
      LOG_WARN("gtk_icon_theme_load_icon failed: [%d] %s", error->code, error->message);
      g_error_free(error);
      error = nullptr;
    } else {
      opixbuf = GDK_PIXBUF(g_object_ref(cpixbuf));
    }
    g_free(file);
  } else {
    cpixbuf = gtk_icon_theme_load_icon(theme, "tip-hide", MAX_ICONSIZE,
                                       GtkIconLookupFlags(0), NULL);
    opixbuf = gtk_icon_theme_load_icon(theme, "tip-show", MAX_ICONSIZE,
                                       GtkIconLookupFlags(0), NULL);
  }

  /* 创建主信息 */
  if (grpinf->type == GROUP_BELONG_TYPE_REGULAR) {
    pal = (PalInfo *)grpinf->member->data;
    inet_ntop(AF_INET, &pal->ipv4, ipstr, INET_ADDRSTRLEN);
    info = g_strdup_printf("%s\n%s", pal->name, ipstr);
  } else
    info = g_strdup(grpinf->name);

  /* 创建扩展信息 */
  if (grpinf->type == GROUP_BELONG_TYPE_REGULAR)
    extra = NULL;
  else
    extra = g_strdup_printf("(%u)", g_slist_length(grpinf->member));

  /* 创建字体风格 */
  attrs = pango_attr_list_new();
  if (grpinf->type == GROUP_BELONG_TYPE_REGULAR) {
    dspt = pango_font_description_from_string(progdt.font);
    attr = pango_attr_font_desc_new(dspt);
    pango_attr_list_insert(attrs, attr);
    pango_font_description_free(dspt);
  } else {
    attr = pango_attr_size_new(8192);
    pango_attr_list_insert(attrs, attr);
    attr = pango_attr_style_new(PANGO_STYLE_ITALIC);
    pango_attr_list_insert(attrs, attr);
    attr = pango_attr_weight_new(PANGO_WEIGHT_BOLD);
    pango_attr_list_insert(attrs, attr);
  }

  /* 设置相应的数据 */
  gtk_tree_store_set(GTK_TREE_STORE(model), iter, 0, cpixbuf, 1, opixbuf, 2,
                     info, 3, extra, 4, attrs, 5, &color, 6, grpinf, -1);

  /* 释放资源 */
  if (cpixbuf) g_object_unref(cpixbuf);
  if (opixbuf) g_object_unref(opixbuf);
  g_free(info);
  g_free(extra);
  pango_attr_list_unref(attrs);
}

/**
 * 更新群组数据(grpinf)到数据集(model)指定位置(iter).
 * @param model model
 * @param iter iter
 * @param grpinf class GroupInfo
 */
void MainWindow::UpdateGroupInfoToPaltree(GtkTreeModel *model,
                                          GtkTreeIter *iter,
                                          GroupInfo *grpinf) {
  GtkIconTheme *theme;
  GdkPixbuf *cpixbuf, *opixbuf;
  PangoAttrList *attrs;
  PangoAttribute *attr;
  PangoFontDescription *dspt;
  char ipstr[INET_ADDRSTRLEN];
  gchar *file, *info, *extra;
  PalInfo *pal;

  /* 创建图标 */
  cpixbuf = opixbuf = NULL;
  theme = gtk_icon_theme_get_default();
  if (grpinf->type == GROUP_BELONG_TYPE_REGULAR) {
    pal = (PalInfo *)grpinf->member->data;
    file = iptux_erase_filename_suffix(pal->iconfile);
    cpixbuf = gtk_icon_theme_load_icon(theme, file, MAX_ICONSIZE,
                                       GtkIconLookupFlags(0), NULL);
    opixbuf = GDK_PIXBUF(g_object_ref(cpixbuf));
    g_free(file);
  }

  /* 创建主信息 */
  info = NULL;
  if (grpinf->type == GROUP_BELONG_TYPE_REGULAR) {
    pal = (PalInfo *)grpinf->member->data;
    inet_ntop(AF_INET, &pal->ipv4, ipstr, INET_ADDRSTRLEN);
    info = g_strdup_printf("%s\n%s", pal->name, ipstr);
  }

  /* 创建扩展信息 */
  extra = NULL;
  if (grpinf->type != GROUP_BELONG_TYPE_REGULAR)
    extra = g_strdup_printf("(%u)", g_slist_length(grpinf->member));

  /* 创建字体风格 */
  attrs = NULL;
  if (grpinf->type == GROUP_BELONG_TYPE_REGULAR) {
    attrs = pango_attr_list_new();
    dspt = pango_font_description_from_string(progdt.font);
    attr = pango_attr_font_desc_new(dspt);
    pango_attr_list_insert(attrs, attr);
    pango_font_description_free(dspt);
  }

  /* 设置相应的数据 */
  if (grpinf->type == GROUP_BELONG_TYPE_REGULAR)
    gtk_tree_store_set(GTK_TREE_STORE(model), iter, 0, cpixbuf, 1, opixbuf, 2,
                       info, 4, attrs, -1);
  else
    gtk_tree_store_set(GTK_TREE_STORE(model), iter, 3, extra, -1);

  /* 释放资源 */
  if (cpixbuf) g_object_unref(cpixbuf);
  if (opixbuf) g_object_unref(opixbuf);
  g_free(info);
  g_free(extra);
  if (attrs) pango_attr_list_unref(attrs);
}

/**
 * 闪烁指定项.
 * @param model model
 * @param iter iter
 * @param blinking 是否继续闪烁
 */
void MainWindow::BlinkGroupItemToPaltree(GtkTreeModel *model, GtkTreeIter *iter,
                                         bool blinking) {
  static GdkColor color1 = {0xff, 0x5252, 0xb8b8, 0x3838},
                  color2 = {0xff, 0x0000, 0x0000, 0xffff};
  GdkColor *color;

  if (blinking) {
    gtk_tree_model_get(model, iter, 5, &color, -1);
    if ((color->red == color1.red) && (color->green == color1.green) &&
        (color->blue == color1.blue))
      gtk_tree_store_set(GTK_TREE_STORE(model), iter, 5, &color2, -1);
    else
      gtk_tree_store_set(GTK_TREE_STORE(model), iter, 5, &color1, -1);
  } else
    gtk_tree_store_set(GTK_TREE_STORE(model), iter, 5, &color1, -1);
}

/**
 * 为好友树(paltree)创建弹出菜单.
 * @param grpinf 好友群组信息
 * @return 菜单
 */
GtkWidget *MainWindow::CreatePaltreePopupMenu(GroupInfo *grpinf) {
  GtkWidget *menu, *menuitem;

  menu = gtk_menu_new();

  /* 发送消息菜单 */
  NO_OPERATION_C
  menuitem = gtk_menu_item_new_with_label(_("Send Message"));
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
  switch (grpinf->type) {
    case GROUP_BELONG_TYPE_REGULAR:
      g_signal_connect_swapped(
          menuitem, "activate",
          G_CALLBACK(onPaltreePopupMenuSendMessageActivateRegular), grpinf);
      break;
    case GROUP_BELONG_TYPE_SEGMENT:
    case GROUP_BELONG_TYPE_GROUP:
    case GROUP_BELONG_TYPE_BROADCAST:
      g_signal_connect_swapped(
          menuitem, "activate",
          G_CALLBACK(onPaltreePopupMenuSendMessageActivateGroup), grpinf);
      break;
    default:
      gtk_widget_set_sensitive(menuitem, FALSE);
      break;
  }

  /* 请求共享文件菜单 */
  NO_OPERATION_C
  menuitem = gtk_menu_item_new_with_label(_("Request Shared Resources"));
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
  switch (grpinf->type) {
    case GROUP_BELONG_TYPE_REGULAR:
      g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(AskSharedFiles),
                               grpinf);
      break;
    default:
      gtk_widget_set_sensitive(menuitem, FALSE);
      break;
  }

  /* 改变好友信息数据菜单 */
  NO_OPERATION_C
  menuitem = gtk_menu_item_new_with_label(_("Change Info."));
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
  switch (grpinf->type) {
    case GROUP_BELONG_TYPE_REGULAR:
      g_signal_connect_swapped(menuitem, "activate",
                               G_CALLBACK(RevisePal::ReviseEntry),
                               grpinf->member->data);
      break;
    default:
      gtk_widget_set_sensitive(menuitem, FALSE);
      break;
  }

  /* 删除好友项菜单 */
  NO_OPERATION_C
  menuitem = gtk_menu_item_new_with_label(_("Delete Pal"));
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
  switch (grpinf->type) {
    case GROUP_BELONG_TYPE_REGULAR:
      g_signal_connect_swapped(menuitem, "activate", G_CALLBACK(DeletePalItem),
                               grpinf);
      break;
    default:
      gtk_widget_set_sensitive(menuitem, FALSE);
      break;
  }

  return menu;
}

/**
 * 填充好友数据到TextBuffer.
 * @param buffer text-buffer
 * @param pal class PalInfo
 */
void MainWindow::FillPalInfoToBuffer(GtkTextBuffer *buffer, PalInfo *pal) {
  char buf[MAX_BUFLEN], ipstr[INET_ADDRSTRLEN];
  GtkTextIter iter;

  gtk_text_buffer_get_end_iter(buffer, &iter);

  snprintf(buf, MAX_BUFLEN, _("Version: %s\n"), pal->version);
  gtk_text_buffer_insert(buffer, &iter, buf, -1);

  if (pal->group && *pal->group != '\0')
    snprintf(buf, MAX_BUFLEN, _("Nickname: %s@%s\n"), pal->name, pal->group);
  else
    snprintf(buf, MAX_BUFLEN, _("Nickname: %s\n"), pal->name);
  gtk_text_buffer_insert(buffer, &iter, buf, -1);

  snprintf(buf, MAX_BUFLEN, _("User: %s\n"), pal->user);
  gtk_text_buffer_insert(buffer, &iter, buf, -1);

  snprintf(buf, MAX_BUFLEN, _("Host: %s\n"), pal->host);
  gtk_text_buffer_insert(buffer, &iter, buf, -1);

  inet_ntop(AF_INET, &pal->ipv4, ipstr, INET_ADDRSTRLEN);
  if (pal->segdes && *pal->segdes != '\0')
    snprintf(buf, MAX_BUFLEN, _("Address: %s(%s)\n"), pal->segdes, ipstr);
  else
    snprintf(buf, MAX_BUFLEN, _("Address: %s\n"), ipstr);
  gtk_text_buffer_insert(buffer, &iter, buf, -1);

  if (!FLAG_ISSET(pal->flags, 0))
    snprintf(buf, MAX_BUFLEN, "%s", _("Compatibility: Microsoft\n"));
  else
    snprintf(buf, MAX_BUFLEN, "%s", _("Compatibility: GNU/Linux\n"));
  gtk_text_buffer_insert(buffer, &iter, buf, -1);

  snprintf(buf, MAX_BUFLEN, _("System coding: %s\n"), pal->encode);
  gtk_text_buffer_insert(buffer, &iter, buf, -1);

  if (pal->sign && *pal->sign != '\0') {
    gtk_text_buffer_insert(buffer, &iter, _("Signature:\n"), -1);
    gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, pal->sign, -1,
                                             "sign-words", NULL);
  }
}

/**
 * 更新UI.
 * @param mwin 主窗口类
 * @return Gtk+库所需
 */
gboolean MainWindow::UpdateUI(MainWindow *mwin) {
  static uint32_t sumonline = 0;  //避免每次都作一次设置
  GtkWidget *widget;
  char buf[MAX_BUFLEN];
  uint32_t sum;
  GSList *tlist;

  /* 统计当前在线人数 */
  sum = 0;
  tlist = g_cthrd->GetPalList();
  while (tlist) {
    if (FLAG_ISSET(((PalInfo *)tlist->data)->flags, 1)) sum++;
    tlist = g_slist_next(tlist);
  }

  /* 更新UI */
  if (sumonline != sum) {
    snprintf(buf, MAX_BUFLEN, _("Pals Online: %" PRIu32), sum);
    widget =
        GTK_WIDGET(g_datalist_get_data(&mwin->widset, "online-label-widget"));
    gtk_label_set_text(GTK_LABEL(widget), buf);
    sumonline = sum;
  }

  return TRUE;
}

/**
 * 转到上一类结构树.
 * @param mwin 主窗口类
 */
void MainWindow::GoPrevTreeModel(MainWindow *mwin) {
  GtkWidget *widget;
  GtkTreeModel *model;
  GtkTreeViewColumn *column;
  GList *tlist;

  widget =
      GTK_WIDGET(g_datalist_get_data(&mwin->widset, "paltree-treeview-widget"));
  model = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));
  if ((tlist = g_list_find(mwin->tmdllist, model))) {
    if (tlist->prev)
      model = GTK_TREE_MODEL(tlist->prev->data);
    else
      model = GTK_TREE_MODEL(g_list_last(mwin->tmdllist)->data);
    gtk_tree_view_set_model(GTK_TREE_VIEW(widget), model);
  }
  column =
      GTK_TREE_VIEW_COLUMN(g_object_get_data(G_OBJECT(widget), "info-column"));
  gtk_tree_view_column_queue_resize(column);
}

/**
 * 转到下一类结构树.
 * @param mwin 主窗口类
 */
void MainWindow::GoNextTreeModel(MainWindow *mwin) {
  GtkWidget *widget;
  GtkTreeModel *model;
  GtkTreeViewColumn *column;
  GList *tlist;

  widget =
      GTK_WIDGET(g_datalist_get_data(&mwin->widset, "paltree-treeview-widget"));
  model = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));
  if ((tlist = g_list_find(mwin->tmdllist, model))) {
    if (tlist->next)
      model = GTK_TREE_MODEL(tlist->next->data);
    else
      model = GTK_TREE_MODEL(mwin->tmdllist->data);
    gtk_tree_view_set_model(GTK_TREE_VIEW(widget), model);
  }
  column =
      GTK_TREE_VIEW_COLUMN(g_object_get_data(G_OBJECT(widget), "info-column"));
  gtk_tree_view_column_queue_resize(column);
}

/**
 * 更新文件传输窗口UI.
 * @param treeview tree-view
 * @return GLib库所需
 */
gboolean MainWindow::UpdateTransUI(GtkWidget *treeview) {
  GtkTreeModel *model;
  GtkTreeIter iter;
  TransAbstract *trans;
  GData **para;

  /* 考察是否需要更新UI */
  model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview));
  if (!gtk_tree_model_get_iter_first(model, &iter)) return TRUE;

  /* 更新UI */
  do {
    gtk_tree_model_get(model, &iter, TRANS_TREE_MAX - 1, &trans, -1);
    if (trans) {  //当文件传输类存在时才能更新
      para = trans->GetTransFilePara();  //获取参数
      /* 更新数据 */
      gtk_list_store_set(
          GTK_LIST_STORE(model), &iter, 0, g_datalist_get_data(para, "status"),
          1, g_datalist_get_data(para, "task"), 2,
          g_datalist_get_data(para, "peer"), 3, g_datalist_get_data(para, "ip"),
          4, g_datalist_get_data(para, "filename"), 5,
          g_datalist_get_data(para, "filelength"), 6,
          g_datalist_get_data(para, "finishlength"), 7,
          GPOINTER_TO_INT(g_datalist_get_data(para, "progress")), 8,
          g_datalist_get_data(para, "pro-text"), 9,
          g_datalist_get_data(para, "cost"), 10,
          g_datalist_get_data(para, "remain"), 11,
          g_datalist_get_data(para, "rate"), 13,
          g_datalist_get_data(para, "data"), -1);
    }
  } while (gtk_tree_model_iter_next(model, &iter));

  /* 重新调整UI */
  gtk_tree_view_columns_autosize(GTK_TREE_VIEW(treeview));

  return TRUE;
}


/**
 * 更新好友树.
 * @param mwin 主窗口类
 */
void MainWindow::onRefresh(void*, void*, MainWindow& self) {
  auto mwin = &self;

  pthread_t pid;

  g_cthrd->Lock();
  mwin->ClearAllItemFromPaltree();
  g_cthrd->ClearAllPalFromList();
  g_cthrd->Unlock();

  pthread_create(&pid, NULL, ThreadFunc(CoreThread::SendNotifyToAll), g_cthrd);
  pthread_detach(pid);
}

void MainWindow::onDetect(void*, void*, MainWindow& self) {
  DetectPal::DetectEntry(self.window);
}


void MainWindow::onSortBy(void *, GVariant* value, MainWindow& self) {
  string sortBy = g_variant_get_string(value, nullptr);

  GtkTreeIterCompareFunc func;

  if(sortBy == "nickname") {
    func = (GtkTreeIterCompareFunc)PaltreeCompareByNameFunc;
  } else if(sortBy == "ip") {
    func = (GtkTreeIterCompareFunc)PaltreeCompareByIPFunc;
  } else {
    LOG_WARN("unknown sort by: %s", sortBy.c_str());
    return;
  }


  GtkTreeModel *model;

  model = GTK_TREE_MODEL(g_datalist_get_data(&self.mdlset, "regular-paltree-model"));
  gtk_tree_sortable_set_default_sort_func(GTK_TREE_SORTABLE(model), func, NULL,
                                          NULL);

  model = GTK_TREE_MODEL(g_datalist_get_data(&self.mdlset, "segment-paltree-model"));
  gtk_tree_sortable_set_default_sort_func(GTK_TREE_SORTABLE(model), func, NULL,
                                          NULL);

  model = GTK_TREE_MODEL(g_datalist_get_data(&self.mdlset, "group-paltree-model"));
  gtk_tree_sortable_set_default_sort_func(GTK_TREE_SORTABLE(model), func, NULL,
                                          NULL);

  model =
      GTK_TREE_MODEL(g_datalist_get_data(&self.mdlset, "broadcast-paltree-model"));
  gtk_tree_sortable_set_default_sort_func(GTK_TREE_SORTABLE(model), func, NULL,
                                          NULL);

}


void MainWindow::onSortType(void *, GVariant *value, MainWindow &self) {
  string sortType = g_variant_get_string(value, nullptr);

  GtkSortType type;

  if(sortType == "ascending") {
    type = GTK_SORT_ASCENDING;
  } else if(sortType == "descending") {
    type = GTK_SORT_DESCENDING;
  } else {
    LOG_WARN("unknown sorttype: %s", sortType.c_str());
    return;
  }

  GtkTreeModel *model;

  model = GTK_TREE_MODEL(g_datalist_get_data(&self.mdlset, "regular-paltree-model"));
  gtk_tree_sortable_set_sort_column_id(
      GTK_TREE_SORTABLE(model), GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, type);

  model = GTK_TREE_MODEL(g_datalist_get_data(&self.mdlset, "segment-paltree-model"));
  gtk_tree_sortable_set_sort_column_id(
      GTK_TREE_SORTABLE(model), GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, type);

  model = GTK_TREE_MODEL(g_datalist_get_data(&self.mdlset, "group-paltree-model"));
  gtk_tree_sortable_set_sort_column_id(
      GTK_TREE_SORTABLE(model), GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, type);

  model =
      GTK_TREE_MODEL(g_datalist_get_data(&self.mdlset, "broadcast-paltree-model"));
  gtk_tree_sortable_set_sort_column_id(
      GTK_TREE_SORTABLE(model), GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, type);
}

/**
 * 请求此好友的共享文件.
 * @param grpinf 好友群组信息
 */
void MainWindow::AskSharedFiles(GroupInfo *grpinf) {
  Command cmd;

  cmd.SendAskShared(g_cthrd->getUdpSock(), (PalInfo *)grpinf->member->data, 0,
                    NULL);
}

/**
 * 删除好友项.
 * @param grpinf 好友群组信息
 */
void MainWindow::DeletePalItem(GroupInfo *grpinf) {
  PalInfo *pal;

  /* 从UI中移除 */
  if (g_mwin->PaltreeContainItem(grpinf->grpid))
    g_mwin->DelItemFromPaltree(grpinf->grpid);

  g_cthrd->Lock();
  /* 从数据中心点移除 */
  if ((pal = g_cthrd->GetPalFromList(grpinf->grpid))) {
    g_cthrd->DelPalFromList(grpinf->grpid);
    FLAG_CLR(pal->flags, 1);
  }
  /* 加入黑名单 */
  if (!g_cthrd->BlacklistContainItem(grpinf->grpid))
    g_cthrd->AttachItemToBlacklist(grpinf->grpid);
  g_cthrd->Unlock();
}

/**
 * 好友树(paltree)信息提示查询请求.
 * @param treeview the object which received the signal
 * @param x the x coordinate of the cursor position
 * @param y the y coordinate of the cursor position
 * @param key TRUE if the tooltip was trigged using the keyboard
 * @param tooltip a GtkTooltip
 * @return Gtk+库所需
 */
gboolean MainWindow::PaltreeQueryTooltip(GtkWidget *treeview, gint x, gint y,
                                         gboolean key, GtkTooltip *tooltip,
                                         MainWindow *self) {
  GdkColor color = {0xff, 0xffff, 0xffff, 0xd6d8};
  GtkWidget *textview;
  GtkTextBuffer *buffer;
  GtkTreePath *path;
  GtkTreeModel *model;
  GtkTreeIter iter;
  gint bx, by;
  GroupInfo *grpinf;

  gtk_tree_view_convert_widget_to_bin_window_coords(GTK_TREE_VIEW(treeview), x,
                                                    y, &bx, &by);
  if (key || !gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(treeview), bx, by,
                                            &path, NULL, NULL, NULL))
    return FALSE;

  model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview));
  gtk_tree_model_get_iter(model, &iter, path);
  gtk_tree_path_free(path);
  gtk_tree_model_get(model, &iter, 6, &grpinf, -1);
  if (grpinf->type != GROUP_BELONG_TYPE_REGULAR) return FALSE;

  buffer = gtk_text_buffer_new(self->progdt.table);
  FillPalInfoToBuffer(buffer, (PalInfo *)grpinf->member->data);
  textview = gtk_text_view_new_with_buffer(buffer);
  gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(textview), FALSE);
  gtk_text_view_set_editable(GTK_TEXT_VIEW(textview), FALSE);
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(textview), GTK_WRAP_NONE);
  gtk_widget_modify_base(textview, GTK_STATE_NORMAL, &color);
  gtk_tooltip_set_custom(tooltip, textview);
  g_object_unref(buffer);

  return TRUE;
}

/**
 * 好友树(paltree)项被激活.
 * @param treeview the object on which the signal is emitted
 * @param path the GtkTreePath for the activated row
 * @param column the GtkTreeViewColumn in which the activation occurred
 */
void MainWindow::onPaltreeItemActivated(GtkWidget *treeview, GtkTreePath *path,
                                        GtkTreeViewColumn *column,
                                        MainWindow *self) {
  GtkTreeModel *model;
  GtkTreeIter iter;
  GroupInfo *grpinf;

  /* 获取项关联的群组数据 */
  model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview));
  gtk_tree_model_get_iter(model, &iter, path);
  gtk_tree_model_get(model, &iter, 6, &grpinf, -1);
  /* 检查是否需要新建对话框 */
  if (grpinf->dialog) {
    gtk_window_present(GTK_WINDOW(grpinf->dialog));
    return;
  }

  /* 根据需求建立对应的对话框 */
  switch (grpinf->type) {
    case GROUP_BELONG_TYPE_REGULAR:
      DialogPeer::PeerDialogEntry(self, grpinf, self->progdt);
      break;
    case GROUP_BELONG_TYPE_SEGMENT:
    case GROUP_BELONG_TYPE_GROUP:
    case GROUP_BELONG_TYPE_BROADCAST:
      DialogGroup::GroupDialogEntry(self, grpinf, self->progdt);
    default:
      break;
  }
}

/**
 * 好友树(paltree)弹出操作菜单.
 * @param treeview tree-view
 * @param event event
 * @return Gtk+库所需
 */
gboolean MainWindow::PaltreePopupMenu(GtkWidget *treeview,
                                      GdkEventButton *event) {
  GtkWidget *menu;
  GtkTreeModel *model;
  GtkTreePath *path;
  GtkTreeIter iter;
  GroupInfo *grpinf;

  /* 检查事件是否可用 */
  if (event->button != 3 ||
      !gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(treeview), GINT(event->x),
                                     GINT(event->y), &path, NULL, NULL, NULL))
    return FALSE;

  /* 获取好友群组信息数据 */
  model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview));
  gtk_tree_model_get_iter(model, &iter, path);
  gtk_tree_path_free(path);
  gtk_tree_model_get(model, &iter, 6, &grpinf, -1);

  /* 弹出菜单 */
  menu = CreatePaltreePopupMenu(grpinf);
  gtk_widget_show_all(menu);
  gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button,
                 event->time);

  return TRUE;
}

/**
 * 展开或隐藏某行.
 * @param treeview text-view
 * @param event event
 * @return Gtk+库所需
 */
gboolean MainWindow::PaltreeChangeStatus(GtkWidget *treeview,
                                         GdkEventButton *event) {
  GtkTreeModel *model;
  GtkCellRenderer *cell;
  GtkTreeViewColumn *column;
  GtkTreePath *path;
  GtkTreeIter iter;
  gint cellx, startpos, width;
  GroupInfo *grpinf;

  /* 检查事件的合法性 */
  if (event->button != 1 || !gtk_tree_view_get_path_at_pos(
                                GTK_TREE_VIEW(treeview), GINT(event->x),
                                GINT(event->y), &path, &column, &cellx, NULL))
    return FALSE;

  /* 检查此行是否可展开 */
  model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview));
  gtk_tree_model_get_iter(model, &iter, path);
  gtk_tree_model_get(model, &iter, 6, &grpinf, -1);
  if (grpinf->type == GROUP_BELONG_TYPE_REGULAR) {
    gtk_tree_path_free(path);
    return FALSE;
  }

  /* 检查事件所发生的位置是否正确 */
  cell =
      GTK_CELL_RENDERER(g_object_get_data(G_OBJECT(column), "expander-cell"));
  gtk_tree_view_column_cell_get_position(column, cell, &startpos, &width);
  if ((cellx < startpos) || (cellx > startpos + width)) {
    gtk_tree_path_free(path);
    return FALSE;
  }

  /* 展开或隐藏行 */
  if (gtk_tree_view_row_expanded(GTK_TREE_VIEW(treeview), path))
    gtk_tree_view_collapse_row(GTK_TREE_VIEW(treeview), path);
  else
    gtk_tree_view_expand_row(GTK_TREE_VIEW(treeview), path, FALSE);
  gtk_tree_path_free(path);

  return TRUE;
}

/**
 * 好友树(paltree)拖拽事件响应处理函数.
 * @param treeview tree-view
 * @param context the drag context
 * @param x where the drop happened
 * @param y where the drop happened
 * @param data the received data
 * @param info the info that has been registered with the target in the
 * GtkTargetList
 * @param time the timestamp at which the data was received
 */
void MainWindow::PaltreeDragDataReceived(GtkWidget *treeview,
                                         GdkDragContext *context, gint x,
                                         gint y, GtkSelectionData *data,
                                         guint info, guint time,
                                         MainWindow *self) {
  GtkTreeModel *model;
  GtkTreePath *path;
  GtkTreeIter iter;
  gint bx, by;
  GroupInfo *grpinf;
  SessionAbstract *session;
  GSList *list;

  /* 事件是否可用 */
  gtk_tree_view_convert_widget_to_bin_window_coords(GTK_TREE_VIEW(treeview), x,
                                                    y, &bx, &by);
  if (!gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(treeview), bx, by, &path,
                                     NULL, NULL, NULL))
    return;

  /* 获取好友群组信息数据 */
  model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview));
  gtk_tree_model_get_iter(model, &iter, path);
  gtk_tree_path_free(path);
  gtk_tree_model_get(model, &iter, 6, &grpinf, -1);

  /* 如果好友群组对话框尚未创建，则先创建对话框 */
  if (!(grpinf->dialog)) {
    switch (grpinf->type) {
      case GROUP_BELONG_TYPE_REGULAR:
        DialogPeer::PeerDialogEntry(self, grpinf, self->progdt);
        break;
      case GROUP_BELONG_TYPE_SEGMENT:
      case GROUP_BELONG_TYPE_GROUP:
      case GROUP_BELONG_TYPE_BROADCAST:
        DialogGroup::GroupDialogEntry(self, grpinf, self->progdt);
      default:
        break;
    }
  } else
    gtk_window_present(GTK_WINDOW(grpinf->dialog));
  /* 获取会话对象，并将数据添加到会话对象 */
  session = (SessionAbstract *)g_object_get_data(G_OBJECT(grpinf->dialog),
                                                 "session-class");
  list = selection_data_get_path(data);  //获取所有文件
  session->AttachEnclosure(list);
  g_slist_foreach(list, GFunc(g_free), NULL);
  g_slist_free(list);
  //        session->ShowEnclosure();
}

/**
 * 好友树(paltree)按昵称排序的比较函数.
 * @param model paltree-model
 * @param a A GtkTreeIter in model
 * @param b Another GtkTreeIter in model
 * @return 比较值
 */
gint MainWindow::PaltreeCompareByNameFunc(GtkTreeModel *model, GtkTreeIter *a,
                                          GtkTreeIter *b) {
  GroupInfo *agrpinf, *bgrpinf;
  gint result;

  gtk_tree_model_get(model, a, 6, &agrpinf, -1);
  gtk_tree_model_get(model, b, 6, &bgrpinf, -1);
  result = strcmp(agrpinf->name, bgrpinf->name);

  return result;
}

/**
 * 好友树(paltree)按IP排序的比较函数.
 * @param model paltree-model
 * @param a A GtkTreeIter in model
 * @param b Another GtkTreeIter in model
 * @return 比较值
 */
gint MainWindow::PaltreeCompareByIPFunc(GtkTreeModel *model, GtkTreeIter *a,
                                        GtkTreeIter *b) {
  GroupInfo *agrpinf, *bgrpinf;
  gint result;

  gtk_tree_model_get(model, a, 6, &agrpinf, -1);
  gtk_tree_model_get(model, b, 6, &bgrpinf, -1);
  if (agrpinf->type == GROUP_BELONG_TYPE_REGULAR &&
      bgrpinf->type == GROUP_BELONG_TYPE_REGULAR)
    result = ntohl(agrpinf->grpid) - ntohl(bgrpinf->grpid);
  else
    result = 0;

  return result;
}

/**
 * 显示好友清单区域.
 * @param widset widget set
 */
void MainWindow::onFind(void*, void*, MainWindow&self) {
  GtkWidget *widget;

  widget = GTK_WIDGET(g_datalist_get_data(&self.widset, "pallist-box-widget"));
  gtk_widget_show(widget);
  widget = GTK_WIDGET(g_datalist_get_data(&self.widset, "pallist-entry-widget"));
  gtk_widget_grab_focus(widget);
  PallistEntryChanged(widget, &self.widset);
}

void MainWindow::onAbout(void*, void*, MainWindow&self) {
  gtk_dialog_run(GTK_DIALOG(HelpDialog::AboutEntry(GTK_WINDOW(self.window))));
}

/**
 * 隐藏好友清单区域.
 * @param widset widget set
 */
void MainWindow::HidePallistArea(GData **widset) {
  GtkWidget *widget;
  GtkTreeModel *model;

  widget = GTK_WIDGET(g_datalist_get_data(widset, "pallist-box-widget"));
  gtk_widget_hide(widget);
  widget = GTK_WIDGET(g_datalist_get_data(widset, "pallist-treeview-widget"));
  model = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));
  gtk_list_store_clear(GTK_LIST_STORE(model));
  widget = GTK_WIDGET(g_datalist_get_data(widset, "pallist-entry-widget"));
  gtk_editable_delete_text(GTK_EDITABLE(widget), 0, -1);
}

/**
 * 清空好友清单搜索输入框.
 * @param entry entry
 * @param event event
 * @return Gtk+库所需
 */
gboolean MainWindow::ClearPallistEntry(GtkWidget *entry, GdkEventKey *event) {
  if (event->keyval != GDK_KEY_Escape) return FALSE;
  gtk_editable_delete_text(GTK_EDITABLE(entry), 0, -1);
  return TRUE;
}

/**
 * 好友清单搜索输入框内容变更响应处理函数.
 * @param entry entry
 * @param widset widget set
 */
void MainWindow::PallistEntryChanged(GtkWidget *entry, GData **widset) {
  GtkIconTheme *theme;
  GdkPixbuf *pixbuf;
  GtkWidget *treeview;
  GtkTreeModel *model;
  GtkTreeIter iter;
  char ipstr[INET_ADDRSTRLEN], *file;
  const gchar *text;
  GSList *tlist;
  PalInfo *pal;

  /* 获取默认主题 */
  theme = gtk_icon_theme_get_default();
  /* 获取搜索内容 */
  text = gtk_entry_get_text(GTK_ENTRY(entry));
  /* 获取好友清单，并清空 */
  treeview = GTK_WIDGET(g_datalist_get_data(widset, "pallist-treeview-widget"));
  model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview));
  gtk_list_store_clear(GTK_LIST_STORE(model));

  /* 将符合条件的好友加入好友清单 */
  tlist = g_cthrd->GetPalList();
  while (tlist) {
    pal = (PalInfo *)tlist->data;
    inet_ntop(AF_INET, &pal->ipv4, ipstr, INET_ADDRSTRLEN);
    /* Search friends case ingore is better. */
    if (*text == '\0' || strcasestr(pal->name, text) ||
        (pal->group && strcasestr(pal->group, text)) ||
        strcasestr(ipstr, text) || strcasestr(pal->user, text) ||
        strcasestr(pal->host, text)) {
      file = iptux_erase_filename_suffix(pal->iconfile);
      pixbuf = gtk_icon_theme_load_icon(theme, file, MAX_ICONSIZE,
                                        GtkIconLookupFlags(0), NULL);
      g_free(file);
      gtk_list_store_append(GTK_LIST_STORE(model), &iter);
      gtk_list_store_set(GTK_LIST_STORE(model), &iter, 0, pixbuf, 1, pal->name,
                         2, pal->group, 3, ipstr, 4, pal->user, 5, pal->host, 6,
                         pal, -1);
      if (pixbuf) g_object_unref(pixbuf);
    }
    tlist = g_slist_next(tlist);
  }

  /* 重新调整好友清单UI */
  gtk_tree_view_columns_autosize(GTK_TREE_VIEW(treeview));
}

/**
 * 好友清单(pallist)项被激活.
 * @param treeview the object on which the signal is emitted
 * @param path the GtkTreePath for the activated row
 * @param column the GtkTreeViewColumn in which the activation occurred
 */
void MainWindow::PallistItemActivated(GtkWidget *treeview, GtkTreePath *path,
                                      GtkTreeViewColumn *column,
                                      MainWindow *self) {
  GtkTreeModel *model;
  GtkTreeIter iter;
  GroupInfo *grpinf;
  PalInfo *pal;

  model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview));
  gtk_tree_model_get_iter(model, &iter, path);
  gtk_tree_model_get(model, &iter, 6, &pal, -1);
  if ((grpinf = g_cthrd->GetPalRegularItem(pal))) {
    if (!(grpinf->dialog))
      DialogPeer::PeerDialogEntry(self, grpinf, self->progdt);
    else
      gtk_window_present(GTK_WINDOW(grpinf->dialog));
  }
}

/**
 * 好友清单(pallist)拖拽事件响应处理函数.
 * @param treeview tree-view
 * @param context the drag context
 * @param x where the drop happened
 * @param y where the drop happened
 * @param data the received data
 * @param info the info that has been registered with the target in the
 * GtkTargetList
 * @param time the timestamp at which the data was received
 */
void MainWindow::PallistDragDataReceived(GtkWidget *treeview,
                                         GdkDragContext *context, gint x,
                                         gint y, GtkSelectionData *data,
                                         guint info, guint time,
                                         MainWindow *self) {
  GtkTreeModel *model;
  GtkTreePath *path;
  GtkTreeIter iter;
  gint bx, by;
  GroupInfo *grpinf;
  PalInfo *pal;
  SessionAbstract *session;
  GSList *list;

  /* 事件是否可用 */
  gtk_tree_view_convert_widget_to_bin_window_coords(GTK_TREE_VIEW(treeview), x,
                                                    y, &bx, &by);
  if (!gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(treeview), bx, by, &path,
                                     NULL, NULL, NULL))
    return;

  /* 获取好友群组信息数据 */
  model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview));
  gtk_tree_model_get_iter(model, &iter, path);
  gtk_tree_path_free(path);
  gtk_tree_model_get(model, &iter, 6, &pal, -1);
  if (!(grpinf = g_cthrd->GetPalRegularItem(pal))) return;

  /* 如果好友群组对话框尚未创建，则先创建对话框 */
  if (!(grpinf->dialog))
    DialogPeer::PeerDialogEntry(self, grpinf, self->progdt);
  else
    gtk_window_present(GTK_WINDOW(grpinf->dialog));
  /* 获取会话对象，并将数据添加到会话对象 */
  session = (SessionAbstract *)g_object_get_data(G_OBJECT(grpinf->dialog),
                                                 "session-class");
  list = selection_data_get_path(data);  //获取所有文件
  session->AttachEnclosure(list);
  g_slist_foreach(list, GFunc(g_free), NULL);
  g_slist_free(list);
  //        session->ShowEnclosure();
}

/**
 * 主窗口位置&大小改变的响应处理函数.
 * @param window 主窗口
 * @param event the GdkEventConfigure which triggered this signal
 * @param dtset data set
 * @return Gtk+库所需
 */
gboolean MainWindow::MWinConfigureEvent(GtkWidget *window,
                                        GdkEventConfigure *event,
                                        MainWindow *self) {
  self->windowConfig.SetWidth(event->width)
      .SetHeight(event->height)
      .SaveToConfig(self->config);
  return FALSE;
}

/**
 * 分割面板的分割位置改变的响应处理函数.
 * @param paned paned
 * @param pspec he GParamSpec of the property which changed
 * @param dtset data set
 */
void MainWindow::PanedDivideChanged(GtkWidget *paned, GParamSpec *pspec,
                                    MainWindow *self) {
  self->config.SetInt("mwin_main_paned_divide",
                      gtk_paned_get_position(GTK_PANED(paned)));
  self->config.Save();
}

gboolean MainWindow::onDeleteEvent(MainWindow *self) {
  return self->statusIcon->AlterInterfaceMode();
}

void MainWindow::onPaltreePopupMenuSendMessageActivateRegular(
    GroupInfo *groupInfo) {
  DialogPeer::PeerDialogEntry(g_mwin, groupInfo, *g_progdt);
}

void MainWindow::onPaltreePopupMenuSendMessageActivateGroup(
    GroupInfo *groupInfo) {
  DialogGroup::GroupDialogEntry(g_mwin, groupInfo, *g_progdt);
}

void MainWindow::onClearChatHistory(void *, void *, MainWindow &self) {
  switch(self.activeWindowType) {
    case ActiveWindowType::PEER:
      ((DialogPeer*)self.activeWindow)->ClearHistoryTextView();
      break;
    case ActiveWindowType::GROUP:
      ((DialogGroup*)self.activeWindow)->ClearHistoryTextView();
      break;
    default:
      LOG_WARN("ClearChatHistory should be disabled for %d", self.activeWindowType);
  }
}

void MainWindow::onInsertPicture(void *, void *, MainWindow &self) {
  switch(self.activeWindowType) {
    case ActiveWindowType::PEER:
      ((DialogPeer*)self.activeWindow)->insertPicture();
      break;
    default:
      LOG_WARN("InsertPicture should be disabled for %d", self.activeWindowType);
  }
}

void MainWindow::onActive(MainWindow& self) {
  if(!gtk_window_is_active(GTK_WINDOW(self.window))) {
    return;
  }
  //self.setActiveWindow(ActiveWindowType::MAIN, &self);
}

void MainWindow::setActiveWindow(ActiveWindowType t, void* activeWindow) {
  this->activeWindowType = t;
  this->activeWindow = activeWindow;
}

void MainWindow::clearActiveWindow(void* activeWindow) {
  if(this->activeWindow == activeWindow) {
    this->activeWindowType = ActiveWindowType ::OTHERS;
    this->activeWindow = nullptr;
  }
}


}  // namespace iptux
