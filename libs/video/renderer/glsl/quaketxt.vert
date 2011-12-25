uniform mat4 mvp_mat;
/** Vertex position.

	x, y, cx, cy

	\a vertex provides the onscreen location at which to draw the character
	(\a x, \a y) and which corner of the character cell this vertex
	represents (\a cx, \a cy). \a cx and \a cy must be either 0 or 1, or
	wierd things will happen with the character cell.
*/
attribute vec4 vertex;

/** The character to draw.

	The quake character map supports only 256 characters, 0-255. Any other
	value will give interesting results.
*/
attribute float char;

/** Coordinate in character map texture.
*/
varying vec2 st;

void
main (void)
{
	float       row, col;
	vec2        pos, corner, uv;
	const vec2  inset = vec2 (0.25, 0.25);
	const vec2  size = vec2 (0.0625, 0.0625);

	row = floor (char / 16.0);
	col = mod (char, 16.0);

	pos = vertex.xy;
	corner = vertex.zw;
	uv = vec2 (row, col) + inset * (1.0 - 2.0 * corner) + 8.0 * corner;
	uv *= size;
	gl_Position = mvp_mat * vec4 (pos + corner * 8.0, 0.0, 1.0);
	st = uv + corner;
}
