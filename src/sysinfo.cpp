#include <config.h>

#include <glib.h>
#include <glib/gi18n.h>

#include <glibtop/fsusage.h>
#include <glibtop/mountlist.h>
#include <glibtop/mem.h>
#include <glibtop/sysinfo.h>

#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>

#include <string>
#include <vector>
#include <fstream>

extern "C" {
#include "sysinfo.h"
#include "util.h"
}


using std::string;
using std::vector;


namespace {


  class SysInfo
  {
  public:
    string hostname;
    string distro_name;
    string distro_version;
    string distro_release;

    guint64 memory_bytes;
    guint64 free_space_bytes;

    guint n_processors;
    vector<string> processors;


    SysInfo(const string &name)
      : distro_name(name),
	distro_version(_("Unknown version"))
    {
      this->load_hostname();
      this->load_processors_info();
      this->load_memory_info();
      this->load_disk_info();
    }

    virtual ~SysInfo()
    { }

  private:

    void load_hostname()
    {
      char buf[256];

      if (gethostname(buf, sizeof buf) == 0)
	if (struct hostent *h = gethostbyname(buf))
	  this->hostname = h->h_name;
    }

    void load_memory_info()
    {
      glibtop_mem mem;

      glibtop_get_mem(&mem);
      this->memory_bytes = mem.total;
    }


    void load_processors_info()
    {
      const glibtop_sysinfo *info = glibtop_get_sysinfo();

      for (guint i = 0; i != info->ncpu; ++i) {
	const char * const keys[] = { "model name", "cpu" };
	gchar *model = 0;

	for (guint j = 0; !model && j != G_N_ELEMENTS(keys); ++j)
	  model = static_cast<char*>(g_hash_table_lookup(info->cpuinfo[i].values,
							 keys[j]));

	if (!model)
	  model = _("Unknown CPU model");

	this->processors.push_back(model);
      }
    }



    void load_disk_info()
    {
      glibtop_mountentry *entries;
      glibtop_mountlist mountlist;

      entries = glibtop_get_mountlist(&mountlist, 0);

      this->free_space_bytes = 0;

      for (guint i = 0; i != mountlist.number; ++i) {
	glibtop_fsusage usage;
	glibtop_get_fsusage(&usage, entries[i].mountdir);
	this->free_space_bytes += usage.bfree * usage.block_size;
      }

      g_free(entries);
    }
  };


  class DebianSysInfo
    : public SysInfo
  {
  public:
    DebianSysInfo()
      : SysInfo("Debian")
    {
      this->load_debian_info();
    }

  private:
    void load_debian_info()
    {
      std::ifstream input("/etc/debian_version");

      if (input)
	std::getline(input, this->distro_version);
      else
	this->distro_version = _("Unknown version");
    }
  };


  class FedoraSysInfo
    : public SysInfo
  {
  public:
    FedoraSysInfo()
      : SysInfo("Fedora")
    {
      this->load_fedora_info();
    }

  private:
    void load_fedora_info()
    {
      std::ifstream input("/etc/fedora-release");

      if (input)
	std::getline(input, this->distro_version);
      else
	this->distro_version = _("Unknown version");
    }
  };


  SysInfo* get_sysinfo()
  {
    if (g_file_test("/etc/debian_version", G_FILE_TEST_EXISTS))
      return new DebianSysInfo;
    else if (g_file_test("/etc/fedora-release", G_FILE_TEST_EXISTS))
      return new FedoraSysInfo;
    else
      return new SysInfo(_("Unknown distro"));
  }
}


GtkWidget *
procman_create_sysinfo_view(void)
{
  GtkWidget *hbox;
  GtkWidget *vbox;

  SysInfo *data = get_sysinfo();;

  GtkWidget *logo;

  GtkWidget *distro_frame;
  GtkWidget *distro_version_label;

  GtkWidget *hardware_frame;
  GtkWidget *hardware_table;
  GtkWidget *memory_label;
  GtkWidget *processor_label;

  GtkWidget *disk_space_frame;
  GtkWidget *disk_space_table;
  GtkWidget *disk_space_label;

  GtkWidget *header;
  GtkWidget *alignment;

  gchar *markup;


  hbox = gtk_hbox_new(FALSE, 12);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 6);

  /* left-side logo */

  logo = gtk_image_new_from_file(DATADIR "/pixmaps/gnome-system-monitor/side.png");
  gtk_misc_set_alignment(GTK_MISC(logo), 0.5, 0.0);
  gtk_misc_set_padding(GTK_MISC(logo), 5, 12);
  gtk_box_pack_start(GTK_BOX(hbox), logo, FALSE, FALSE, 0);

  vbox = gtk_vbox_new(FALSE, 12);
  gtk_container_set_border_width(GTK_CONTAINER(vbox), 12);
  gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);

  // hostname

  markup = g_strdup_printf("<big><big><b><u>%s</u></b></big></big>",
    data->hostname.c_str());
  GtkWidget *hostname_frame = gtk_frame_new(markup);
  g_free(markup);
  gtk_frame_set_label_align(GTK_FRAME(hostname_frame), 0.0, 0.5);
  gtk_label_set_use_markup(
			   GTK_LABEL(gtk_frame_get_label_widget(GTK_FRAME(hostname_frame))),
			   TRUE
			   );
  gtk_frame_set_shadow_type(GTK_FRAME(hostname_frame), GTK_SHADOW_NONE);
  gtk_box_pack_start(GTK_BOX(vbox), hostname_frame, FALSE, FALSE, 0);


  /* distro section */

  markup = g_strdup_printf("<big><big><b>%s</b></big></big>",
    data->distro_name.c_str());
  distro_frame = gtk_frame_new(markup);
  g_free(markup);
  gtk_frame_set_label_align(GTK_FRAME(distro_frame), 0.0, 0.5);
  gtk_label_set_use_markup(
			   GTK_LABEL(gtk_frame_get_label_widget(GTK_FRAME(distro_frame))),
			   TRUE
			   );
  gtk_frame_set_shadow_type(GTK_FRAME(distro_frame), GTK_SHADOW_NONE);
  gtk_box_pack_start(GTK_BOX(vbox), distro_frame, FALSE, FALSE, 0);

  alignment = gtk_alignment_new(0.5, 0.5, 1.0, 1.0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(alignment), 0, 0, 12, 0);
  gtk_container_add(GTK_CONTAINER(distro_frame), alignment);

  if (data->distro_release != "")
    markup = g_strdup_printf(
			     _("Version %s (Release %s)"),
			     data->distro_version.c_str(),
			     data->distro_release.c_str());
  else
    markup = g_strdup_printf(
			     _("Version %s"),
			     data->distro_version.c_str());
  distro_version_label = gtk_label_new(markup);
  g_free(markup);
  gtk_misc_set_alignment(GTK_MISC(distro_version_label), 0.0, 0.5);
  gtk_misc_set_padding(GTK_MISC(distro_version_label), 6, 6);
  gtk_container_add(GTK_CONTAINER(alignment), distro_version_label);

  /* hardware section */

  markup = g_strdup_printf(_("<b>Hardware</b>"));
  hardware_frame = gtk_frame_new(markup);
  g_free(markup);
  gtk_frame_set_label_align(GTK_FRAME(hardware_frame), 0.0, 0.5);
  gtk_label_set_use_markup(
			   GTK_LABEL(gtk_frame_get_label_widget(GTK_FRAME(hardware_frame))),
			   TRUE
			   );
  gtk_frame_set_shadow_type(GTK_FRAME(hardware_frame), GTK_SHADOW_NONE);
  gtk_box_pack_start(GTK_BOX(vbox), hardware_frame, FALSE, FALSE, 0);

  alignment = gtk_alignment_new(0.5, 0.5, 1.0, 1.0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(alignment), 0, 0, 12, 0);
  gtk_container_add(GTK_CONTAINER(hardware_frame), alignment);

  hardware_table = gtk_table_new(data->processors.size(), 2, FALSE);
  gtk_table_set_row_spacings(GTK_TABLE(hardware_table), 6);
  gtk_table_set_col_spacings(GTK_TABLE(hardware_table), 6);
  gtk_container_set_border_width(GTK_CONTAINER(hardware_table), 6);
  gtk_container_add(GTK_CONTAINER(alignment), hardware_table);

  header = gtk_label_new(_("Memory:"));
  gtk_misc_set_alignment(GTK_MISC(header), 0.0, 0.5);
  gtk_table_attach(
		   GTK_TABLE(hardware_table), header,
		   0, 1, 0, 1,
		   GTK_FILL, GTK_FILL, 0, 0
		   );

  markup = SI_gnome_vfs_format_file_size_for_display(data->memory_bytes);
  memory_label = gtk_label_new(markup);
  g_free(markup);
  gtk_misc_set_alignment(GTK_MISC(memory_label), 0.0, 0.5);
  gtk_table_attach(
		   GTK_TABLE(hardware_table), memory_label,
		   1, 2, 0, 1,
		   GTK_FILL, GTK_FILL, 0, 0
		   );

  for (guint i = 0; i < data->processors.size(); ++i) {
    if (data->processors.size() > 1) {
      markup = g_strdup_printf(_("Processor %d:"), i);
      header = gtk_label_new(markup);
      g_free(markup);
    }
    else
      header = gtk_label_new(_("Processor:"));

    gtk_misc_set_alignment(GTK_MISC(header), 0.0, 0.5);
    gtk_table_attach(
		     GTK_TABLE(hardware_table), header,
		     0, 1, 1 + i, 2 + i,
		     GTK_FILL, GTK_FILL, 0, 0
		     );

    processor_label = gtk_label_new(data->processors[i].c_str());
    gtk_misc_set_alignment(GTK_MISC(processor_label), 0.0, 0.5);
    gtk_table_attach(
		     GTK_TABLE(hardware_table), processor_label,
		     1, 2, 1 + i, 2 + i,
		     GTK_FILL, GTK_FILL, 0, 0
		     );
  }

  /* disk space section */

  markup = g_strdup_printf(_("<b>System Status</b>"));
  disk_space_frame = gtk_frame_new(markup);
  g_free(markup);
  gtk_frame_set_label_align(GTK_FRAME(disk_space_frame), 0.0, 0.5);
  gtk_label_set_use_markup(
			   GTK_LABEL(gtk_frame_get_label_widget(GTK_FRAME(disk_space_frame))),
			   TRUE
			   );
  gtk_frame_set_shadow_type(GTK_FRAME(disk_space_frame), GTK_SHADOW_NONE);
  gtk_box_pack_start(GTK_BOX(vbox), disk_space_frame, FALSE, FALSE, 0);

  alignment = gtk_alignment_new(0.5, 0.5, 1.0, 1.0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(alignment), 0, 0, 12, 0);
  gtk_container_add(GTK_CONTAINER(disk_space_frame), alignment);

  disk_space_table = gtk_table_new(1, 2, FALSE);
  gtk_table_set_row_spacings(GTK_TABLE(disk_space_table), 6);
  gtk_table_set_col_spacings(GTK_TABLE(disk_space_table), 6);
  gtk_container_set_border_width(GTK_CONTAINER(disk_space_table), 6);
  gtk_container_add(GTK_CONTAINER(alignment), disk_space_table);

  header = gtk_label_new(_("User Space Free:"));
  gtk_misc_set_alignment(GTK_MISC(header), 0.0, 0.5);
  gtk_table_attach(
		   GTK_TABLE(disk_space_table), header,
		   0, 1, 0, 1,
		   GTK_FILL, GTK_FILL, 0, 0
		   );

  markup = SI_gnome_vfs_format_file_size_for_display(data->free_space_bytes);
  disk_space_label = gtk_label_new(markup);
  g_free(markup);
  gtk_misc_set_alignment(GTK_MISC(disk_space_label), 0.0, 0.5);
  gtk_table_attach(
		   GTK_TABLE(disk_space_table), disk_space_label,
		   1, 2, 0, 1,
		   GTK_FILL, GTK_FILL, 0, 0
		   );

  return hbox;
}
