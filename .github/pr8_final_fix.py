from pathlib import Path

p = Path("src/ui/file_manager_window.cpp")
s = p.read_text()

def one(old, new, label):
    global s
    c = s.count(old)
    if c != 1:
        raise SystemExit(f"{label}: expected 1 match, got {c}")
    s = s.replace(old, new, 1)

one('#include <infiltratr/design.h>\n',
    '#include <infiltratr/design.h>\n#include <infiltratr/temporal_posix.h>\n',
    'temporal include')

one('''    "standard::content-type,standard::icon,standard::is-hidden,standard::is-symlink,"
    "time::modified";
''',
'''    "standard::content-type,standard::icon,standard::is-hidden,standard::is-symlink,"
    "time::modified,time::modified-usec,time::modified-nsec";
''', 'directory attributes')

one('''    build_ui(application);
    apply_theme();

    g_object_weak_ref''',
'''    build_ui(application);
    apply_theme();
    arm_temporal_policy_monitor();

    g_object_weak_ref''', 'arm monitor')

one('''    // Stop mount callbacks before any UI/model references owned by this object are released.
    mounted_places_monitor_.reset();

    if (primary_selection_ != nullptr) {
''',
'''    // Stop external callbacks before any UI/model references owned by this object are released.
    mounted_places_monitor_.reset();
    if (temporal_policy_monitor_ != nullptr) {
        g_signal_handlers_disconnect_by_data(temporal_policy_monitor_, this);
        g_object_unref(temporal_policy_monitor_);
        temporal_policy_monitor_ = nullptr;
    }
    for (GtkWidget *cell : modified_cells_) {
        g_object_weak_unref(
            G_OBJECT(cell), &FileManagerWindow::on_modified_cell_finalized, this);
    }
    modified_cells_.clear();

    if (primary_selection_ != nullptr) {
''', 'destructor callbacks')

one('''        g_signal_connect(factory, "setup", G_CALLBACK(on_factory_setup), this);
        g_signal_connect(factory, "bind", G_CALLBACK(on_factory_bind), this);
''',
'''        g_signal_connect(factory, "setup", G_CALLBACK(on_factory_setup), this);
        g_signal_connect(factory, "bind", G_CALLBACK(on_factory_bind), this);
        g_signal_connect(factory, "unbind", G_CALLBACK(on_factory_unbind), this);
''', 'unbind signal')

setup_start = s.index('void FileManagerWindow::on_factory_setup')
old = '''{
    (void)user_data;

    const int encoded = GPOINTER_TO_INT(
        g_object_get_data(G_OBJECT(factory), "ifm-detail-field"));
'''
pos = s.index(old, setup_start)
s = s[:pos] + s[pos:].replace(old, '''{
    auto *self = static_cast<FileManagerWindow *>(user_data);

    const int encoded = GPOINTER_TO_INT(
        g_object_get_data(G_OBJECT(factory), "ifm-detail-field"));
''', 1)

one('''    gtk_widget_add_css_class(label, "ifm-detail-secondary");
    gtk_list_item_set_child(item, label);
}

void FileManagerWindow::on_factory_bind''',
'''    gtk_widget_add_css_class(label, "ifm-detail-secondary");
    gtk_list_item_set_child(item, label);
    if (field == DetailField::Modified) {
        self->modified_cells_.insert(label);
        g_object_weak_ref(
            G_OBJECT(label), &FileManagerWindow::on_modified_cell_finalized, self);
    }
}

void FileManagerWindow::on_factory_bind''', 'track modified cells')

bind_start = s.index('void FileManagerWindow::on_factory_bind')
old = '''{
    (void)user_data;

    auto *info = G_FILE_INFO(gtk_list_item_get_item(item));
'''
pos = s.index(old, bind_start)
s = s[:pos] + s[pos:].replace(old, '''{
    auto *self = static_cast<FileManagerWindow *>(user_data);

    auto *info = G_FILE_INFO(gtk_list_item_get_item(item));
''', 1)

one('''    case DetailField::Modified: {
        const std::string value = detail_modified_text(info);
        gtk_label_set_text(GTK_LABEL(child), value.c_str());
        break;
    }
    }
}

void FileManagerWindow::on_icon_factory_setup''',
'''    case DetailField::Modified: {
        g_object_set_data_full(
            G_OBJECT(child), "ifm-modified-info", g_object_ref(info),
            reinterpret_cast<GDestroyNotify>(g_object_unref));
        const std::string value = detail_modified_text(info);
        gtk_label_set_text(GTK_LABEL(child), value.c_str());
        break;
    }
    }
}

void FileManagerWindow::on_factory_unbind(GtkSignalListItemFactory *factory,
                                          GtkListItem *item,
                                          gpointer user_data)
{
    (void)user_data;
    const int encoded = GPOINTER_TO_INT(
        g_object_get_data(G_OBJECT(factory), "ifm-detail-field"));
    if (encoded != static_cast<int>(DetailField::Modified)) {
        return;
    }
    if (GtkWidget *child = gtk_list_item_get_child(item); child != nullptr) {
        g_object_set_data_full(
            G_OBJECT(child), "ifm-modified-info", nullptr, nullptr);
    }
}

void FileManagerWindow::on_icon_factory_setup''', 'modified bind data')

marker = 'GFile *FileManagerWindow::file_from_location_text(const char *text) const\n'
if s.count(marker) != 1:
    raise SystemExit('file_from_location_text marker mismatch')
addition = '''void FileManagerWindow::arm_temporal_policy_monitor()
{
    if (temporal_policy_monitor_ != nullptr) {
        g_signal_handlers_disconnect_by_data(temporal_policy_monitor_, this);
        g_object_unref(temporal_policy_monitor_);
        temporal_policy_monitor_ = nullptr;
    }

    char directory[4096]{};
    if (!infiltratr_temporal_posix_policy_directory(directory, sizeof(directory))) {
        return;
    }

    GFile *policy_directory = g_file_new_for_path(directory);
    const bool directory_exists = g_file_test(directory, G_FILE_TEST_IS_DIR);
    temporal_policy_monitoring_parent_ = !directory_exists;
    GFile *target = directory_exists
        ? G_FILE(g_object_ref(policy_directory))
        : g_file_get_parent(policy_directory);
    g_object_unref(policy_directory);
    if (target == nullptr) {
        return;
    }

    GError *error = nullptr;
    temporal_policy_monitor_ = g_file_monitor_directory(
        target, G_FILE_MONITOR_WATCH_MOVES, nullptr, &error);
    g_object_unref(target);
    if (error != nullptr) {
        g_error_free(error);
    }
    if (temporal_policy_monitor_ != nullptr) {
        g_signal_connect(temporal_policy_monitor_, "changed",
                         G_CALLBACK(on_temporal_policy_changed), this);
    }
}

void FileManagerWindow::refresh_modified_cells()
{
    for (GtkWidget *cell : modified_cells_) {
        auto *info = G_FILE_INFO(
            g_object_get_data(G_OBJECT(cell), "ifm-modified-info"));
        if (info == nullptr) {
            continue;
        }
        const std::string value = detail_modified_text(info);
        gtk_label_set_text(GTK_LABEL(cell), value.c_str());
    }
}

void FileManagerWindow::on_temporal_policy_changed(GFileMonitor *monitor,
                                                   GFile *file,
                                                   GFile *other_file,
                                                   GFileMonitorEvent event_type,
                                                   gpointer user_data)
{
    (void)monitor;
    (void)event_type;
    auto *self = static_cast<FileManagerWindow *>(user_data);
    const auto has_basename = [](GFile *candidate, const char *expected) {
        if (candidate == nullptr) {
            return false;
        }
        char *basename = g_file_get_basename(candidate);
        const bool matches = g_strcmp0(basename, expected) == 0;
        g_free(basename);
        return matches;
    };

    if (self->temporal_policy_monitoring_parent_) {
        if (!has_basename(file, "infiltrator") &&
            !has_basename(other_file, "infiltrator")) {
            return;
        }
        self->arm_temporal_policy_monitor();
        self->refresh_modified_cells();
        return;
    }

    if (has_basename(file, "presentation.conf") ||
        has_basename(other_file, "presentation.conf")) {
        self->refresh_modified_cells();
    }
}

void FileManagerWindow::on_modified_cell_finalized(gpointer user_data,
                                                   GObject *where_object_was)
{
    auto *self = static_cast<FileManagerWindow *>(user_data);
    self->modified_cells_.erase(GTK_WIDGET(where_object_was));
}

'''
s = s.replace(marker, addition + marker, 1)
p.write_text(s)

p = Path("src/ui/detail_metadata.cpp")
s = p.read_text()

old = '''bool detail_modified_value(GFileInfo *info, guint64 *value)
{
    if (info == nullptr || value == nullptr ||
        !g_file_info_has_attribute(info, G_FILE_ATTRIBUTE_TIME_MODIFIED)) {
        return false;
    }
    *value = g_file_info_get_attribute_uint64(info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
    return true;
}
'''
new = old + '''
guint32 detail_modified_nanoseconds(GFileInfo *info)
{
    if (info == nullptr) {
        return 0U;
    }
    constexpr const char *kModifiedNsec = "time::modified-nsec";
    constexpr const char *kModifiedUsec = "time::modified-usec";
    if (g_file_info_has_attribute(info, kModifiedNsec)) {
        return g_file_info_get_attribute_uint32(info, kModifiedNsec);
    }
    if (g_file_info_has_attribute(info, kModifiedUsec)) {
        const guint32 microseconds =
            g_file_info_get_attribute_uint32(info, kModifiedUsec);
        return microseconds <= 999999U ? microseconds * 1000U : 0U;
    }
    return 0U;
}
'''
if s.count(old) != 1:
    raise SystemExit("modified helper mismatch")
s = s.replace(old, new, 1)

old = '''    const int by_type = compare_text(left_type.c_str(), right_type.c_str());
    return by_type != 0 ? by_type : name_tiebreak(left, right);
'''
if s.count(old) != 1:
    raise SystemExit("type comparator mismatch")
s = s.replace(old, '''    return compare_text(left_type.c_str(), right_type.c_str());
''', 1)

old = '''    const int by_size = compare_optional_u64(left_present, left_size,
                                             right_present, right_size);
    return by_size != 0 ? by_size : name_tiebreak(left, right);
'''
if s.count(old) != 1:
    raise SystemExit("size comparator mismatch")
s = s.replace(old, '''    return compare_optional_u64(left_present, left_size,
                                right_present, right_size);
''', 1)

old = '''    const int by_modified = compare_optional_u64(left_present, left_modified,
                                                 right_present, right_modified);
    return by_modified != 0 ? by_modified : name_tiebreak(left, right);
'''
if s.count(old) != 1:
    raise SystemExit("modified comparator mismatch")
s = s.replace(old, '''    const int by_seconds = compare_optional_u64(left_present, left_modified,
                                                right_present, right_modified);
    if (by_seconds != 0 || !left_present) {
        return by_seconds;
    }
    const guint32 left_nanoseconds = detail_modified_nanoseconds(left);
    const guint32 right_nanoseconds = detail_modified_nanoseconds(right);
    if (left_nanoseconds < right_nanoseconds) {
        return -1;
    }
    if (left_nanoseconds > right_nanoseconds) {
        return 1;
    }
    return 0;
''', 1)
p.write_text(s)

p = Path("tests/detail_metadata_test.cpp")
s = p.read_text()
old = 'using infiltrator::files::detail_compare_size;\n'
if s.count(old) != 1:
    raise SystemExit("using size mismatch")
s = s.replace(old, old + 'using infiltrator::files::detail_compare_type;\n', 1)

old = '''    }
    g_object_unref(larger);

    // A followed symlink may report the target's regular/directory type. The
'''
new = '''    }

    // Equal primary values must remain equal so GtkColumnViewSorter can consult
    // a secondary sort column. Modified-time ordering also includes subsecond
    // metadata when the filesystem exposes it.
    g_file_info_set_size(larger, 1536);
    g_file_info_set_attribute_uint64(
        larger, G_FILE_ATTRIBUTE_TIME_MODIFIED, 46800U);
    g_file_info_set_attribute_uint32(file, "time::modified-nsec", 100U);
    g_file_info_set_attribute_uint32(larger, "time::modified-nsec", 900U);
    if (detail_compare_type(file, larger) != 0 ||
        detail_compare_size(file, larger) != 0 ||
        detail_compare_modified(file, larger) >= 0) {
        g_object_unref(larger);
        g_free(config_home);
        g_object_unref(file);
        return 14;
    }
    g_file_info_set_attribute_uint32(larger, "time::modified-nsec", 100U);
    if (detail_compare_modified(file, larger) != 0) {
        g_object_unref(larger);
        g_free(config_home);
        g_object_unref(file);
        return 15;
    }
    g_object_unref(larger);

    // A followed symlink may report the target's regular/directory type. The
'''
if s.count(old) != 1:
    raise SystemExit("test insertion mismatch")
s = s.replace(old, new, 1)
p.write_text(s)
