/* mime_type.h -- determine the MIME Content-Type of a file.
 *
 * This code is Copyright (c) 2017, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

/* Return a MIME content-type string for the specified file.
 *
 * If the system supports it, will use the "file" command to determine
 * the appropriate content-type.  Otherwise it will try to determine the
 * content-type from the suffix.  If that fails, the file will be scanned
 * and either assigned a MIME type of text/plain or application/octet-stream
 * depending if binary content is present.
 *
 * Arguments:
 *
 * filename	- The name of the file to determine the MIME type of.
 *
 * Returns a pointer to a content-type string (which may include MIME
 * parameters, such as charset).  Returns a NULL if it cannot determine
 * the MIME type of the file.  Returns allocated storage that must be
 * free'd.
 */
char *mime_type(const char *filename);
