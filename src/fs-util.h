#ifndef DS4_FS_UTIL_H
#define DS4_FS_UTIL_H

#include <string>

namespace ds4fs {

// Writes `content` to `path` atomically and without following symlinks: the
// data goes to a fresh sibling temp file (O_EXCL|O_NOFOLLOW) which is then
// rename()d over `path`. Two properties matter for a root daemon writing under
// directories other users may influence:
//   * a symlink planted at `path` is replaced, never written through, so it
//     cannot redirect the write to an arbitrary file;
//   * a crash or power loss leaves either the old file or the new one, never a
//     truncated half-written config.
// Returns false and sets *error (errno text) on failure.
bool write_file_atomic(const std::string &path, const std::string &content, unsigned mode,
                       std::string *error);

} // namespace ds4fs

#endif // DS4_FS_UTIL_H
