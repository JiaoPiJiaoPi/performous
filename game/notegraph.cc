#include "notegraph.hh"

NoteGraph::NoteGraph(Song const& song):
  m_song(song),
  m_notelines("notelines.svg"),
  m_wave("wave.png"),
  m_notebar("notebar.svg"), m_notebar_hl("notebar.png"),
  m_notebarfs("notebarfs.svg"), m_notebarfs_hl("notebarfs-hl.png"),
  m_notebargold("notebargold.svg"), m_notebargold_hl("notebargold.png"),
  m_notealpha(0.0f), m_nlTop(0.0, 4.0), m_nlBottom(0.0, 4.0), m_time()
{
	m_nlTop.setTarget(m_song.noteMax, true);
	m_nlBottom.setTarget(m_song.noteMin, true);
	reset();
}

void NoteGraph::reset() {
	m_songit = m_song.notes.begin();
}

namespace {
	void drawNotebar(Texture const& texture, double x, double y, double w, double h) {
		UseTexture tblock(texture);
		glBegin(GL_TRIANGLE_STRIP);
		glTexCoord2f(0.0f, 0.0f); glVertex2f(x, y);
		glTexCoord2f(0.0f, 1.0f); glVertex2f(x, y + h);
		if (w >= 2.0 * h) {
			glTexCoord2f(0.5f, 0.0f); glVertex2f(x + h, y);
			glTexCoord2f(0.5f, 1.0f); glVertex2f(x + h, y + h);
			glTexCoord2f(0.5f, 0.0f); glVertex2f(x + w - h, y);
			glTexCoord2f(0.5f, 1.0f); glVertex2f(x + w - h, y + h);
		} else {
			float crop = 0.25f * w / h;
			glTexCoord2f(crop, 0.0f); glVertex2f(x + 0.5 * w, y);
			glTexCoord2f(crop, 1.0f); glVertex2f(x + 0.5 * w, y + h);
			glTexCoord2f(1.0f - crop, 0.0f); glVertex2f(x + 0.5 * w, y);
			glTexCoord2f(1.0f - crop, 1.0f); glVertex2f(x + 0.5 * w, y + h);
		}
		glTexCoord2f(1.0f, 0.0f); glVertex2f(x + w, y);
		glTexCoord2f(1.0f, 1.0f); glVertex2f(x + w, y + h);
		glEnd();
	}
}

const double baseLine = -0.2;
const double pixUnit = 0.2;

void NoteGraph::draw(double time, std::list<Player> const& players) {
	if (time < m_time) reset();
	m_time = time;
	// Update m_songit (which note to start the rendering from)
	while (m_songit != m_song.notes.end() && (m_songit->type == Note::SLEEP || m_songit->end < time - (baseLine + 0.5) / pixUnit)) ++m_songit;

	// Automatically zooming notelines
	{
		int low = m_song.noteMax;
		int high = m_song.noteMin;
		int low2 = m_song.noteMax;
		int high2 = m_song.noteMin;
		for (Notes::const_iterator it = m_songit; it != m_song.notes.end() && it->begin < time + 15.0; ++it) {
			if (it->type == Note::SLEEP) continue;
			if (it->note < low) low = it->note;
			if (it->note > high) high = it->note;
			if (it->begin > time + 8.0) continue;
			if (it->note < low2) low2 = it->note;
			if (it->note > high2) high2 = it->note;
		}
		if (low2 <= high2) {
			m_nlTop.setRange(high2, high);
			m_nlBottom.setRange(low, low2);
		}
	}
	m_max = m_nlTop.get() + 7.0;
	m_min = m_nlBottom.get() - 7.0;
	m_noteUnit = -0.5 / std::max(24.0, m_max - m_min);
	m_baseY = -0.5 * (m_min + m_max) * m_noteUnit;
	m_baseX = baseLine - m_time * pixUnit;

	drawNotes();
	if (config["game/pitch"].b()) drawWaves(players);
}

void NoteGraph::drawNotes() {
	// Draw note lines
	if (m_songit == m_song.notes.end() || m_songit->begin > m_time + 3.0) m_notealpha -= 0.02f;
	else if (m_notealpha < 1.0f) m_notealpha += 0.02f;
	if (m_notealpha <= 0.0f) {
		m_notealpha = 0.0f;
	} else {
		glColor4f(1.0, 1.0, 1.0, m_notealpha);
		m_notelines.draw(Dimensions().stretch(1.0, (m_max - m_min - 13) * m_noteUnit), TexCoords(0.0, (-m_min - 7.0) / 12.0f, 1.0, (-m_max + 6.0) / 12.0f));

		// Draw notes
		for (Notes::const_iterator it = m_songit; it != m_song.notes.end() && it->begin < m_time - (baseLine - 0.5) / pixUnit; ++it) {
			if (it->type == Note::SLEEP) continue;
			double alpha = it->power;
			Texture* t1;
			Texture* t2;
			switch (it->type) {
			  case Note::NORMAL: t1 = &m_notebar; t2 = &m_notebar_hl; break;
			  case Note::GOLDEN: t1 = &m_notebargold; t2 = &m_notebargold_hl; break;
			  case Note::FREESTYLE:  // Freestyle notes use custom handling
				{
					Dimensions dim;
					dim.middle(m_baseX + 0.5 * (it->begin + it->end) * pixUnit).center(m_baseY + it->note * m_noteUnit).stretch((it->end - it->begin) * pixUnit, -m_noteUnit * 12.0);
					float xoffset = 0.1 * m_time / m_notebarfs.ar();
					m_notebarfs.draw(dim, TexCoords(xoffset, 0.0, xoffset + dim.ar() / m_notebarfs.ar(), 1.0));
					if (alpha > 0.0) {
						float xoffset = rand() / double(RAND_MAX);
						m_notebarfs_hl.draw(dim, TexCoords(xoffset, 0.0, xoffset + dim.ar() / m_notebarfs_hl.ar(), 1.0));
					}
				}
				continue;
			  default: throw std::logic_error("Unknown note type: don't know how to render");
			}
			double x = m_baseX + it->begin * pixUnit + m_noteUnit; // left x coordinate: begin minus border (side borders -noteUnit wide)
			double y = m_baseY + (it->note + 1) * m_noteUnit; // top y coordinate (on the one higher note line)
			double w = (it->end - it->begin) * pixUnit - m_noteUnit * 2.0; // width: including borders on both sides
			double h = -m_noteUnit * 2.0; // height: 0.5 border + 1.0 bar + 0.5 border = 2.0
			drawNotebar(*t1, x, y, w, h);
			if (alpha > 0.0) {
				glColor4f(1.0f, 1.0f, 1.0f, alpha * m_notealpha);
				drawNotebar(*t2, x, y, w, h);
				glColor4f(1.0f, 1.0f, 1.0f, m_notealpha);
			}
		}

	}
}

namespace {
	struct Point {
		float tx;
		float ty;
		float vx;
		float vy;
		Point(float tx_, float ty_, float vx_, float vy_): tx(tx_), ty(ty_), vx(vx_), vy(vy_) {}
	};
	
	typedef std::vector<Point> Points;
	
	void strip(Points& points) {
		size_t s = points.size();
		if (s > 3) {
			// Combine the two last points into a terminating point
			{
				Point& p = points[s-2];
				p.ty = 0.5f;
				p.vy = 0.5f * (p.vy + points[s-1].vy);
			}
			points.pop_back();
			// Render them as a triangle stripe
			glEnableClientState(GL_VERTEX_ARRAY);
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			glTexCoordPointer(2, GL_FLOAT, sizeof(Point), &points.front().tx);
			glVertexPointer(2, GL_FLOAT, sizeof(Point), &points.front().vx);
			glDrawArrays(GL_TRIANGLE_STRIP, 0, points.size());
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);
			glDisableClientState(GL_VERTEX_ARRAY);
		}
		points.clear();
	}
}

void NoteGraph::drawWaves(std::list<Player> const& players) {
	UseTexture tblock(m_wave);
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	for (std::list<Player>::const_iterator p = players.begin(); p != players.end(); ++p) {
		glColor4f(p->m_color.r, p->m_color.g, p->m_color.b, m_notealpha);
		float const texOffset = 2.0 * m_time; // Offset for animating the wave texture
		Player::pitch_t const& pitch = p->m_pitch;
		size_t const beginIdx = std::max(0.0, m_time - 0.5 / pixUnit) / Engine::TIMESTEP; // At which pitch idx to start displaying the wave
		size_t const endIdx = p->m_pos;
		size_t idx = beginIdx;
		// Go back until silence (NaN freq) to allow proper wave phase to be calculated
		if (beginIdx < endIdx) while (idx > 0 && pitch[idx].first == pitch[idx].first) --idx;
		// Start processing
		float tex = texOffset;
		double t = idx * Engine::TIMESTEP;
		double oldval = getNaN();
		Points points;
		Notes::const_iterator noteIt = m_song.notes.begin();
		for (; idx < endIdx; ++idx, t += Engine::TIMESTEP) {
			double const freq = pitch[idx].first;
			// If freq is NaN, we have nothing to process
			if (freq != freq) { oldval = getNaN(); tex = texOffset; continue; }
			tex += freq * 0.001; // Wave phase (texture coordinate)
			if (idx < beginIdx) continue; // Skip graphics rendering if out of screen
			double x = -0.2 + (t - m_time) * pixUnit;
			// Find the currently active note(s)
			while (noteIt != m_song.notes.end() && (noteIt->type == Note::SLEEP || t > noteIt->end)) ++noteIt;
			Notes::const_iterator notePrev = noteIt;
			while (notePrev != m_song.notes.begin() && (notePrev->type == Note::SLEEP || t < notePrev->begin)) --notePrev;
			bool hasNote = (noteIt != m_song.notes.end());
			bool hasPrev = (notePrev->type != Note::SLEEP && t >= notePrev->begin);
			double val;
			if (hasNote && hasPrev) val = 0.5 * (noteIt->note + notePrev->note);
			else if (hasNote) val = noteIt->note;
			else val = notePrev->note;
			// Now val contains the active note value. The following calculates note value for current freq:
			val += Note::diff(val, m_song.scale.getNote(freq));
			// Graphics positioning & animation:
			double y = m_baseY + val * m_noteUnit;
			double thickness = clamp(1.0 + pitch[idx].second / 60.0) + 0.5;
			thickness *= 1.0 + 0.2 * std::sin(tex - 2.0 * texOffset); // Further animation :)
			thickness *= -m_noteUnit;
			// If there has been a break or if the pitch change is too fast, terminate and begin a new one
			if (oldval != oldval || std::abs(oldval - val) > 1) strip(points);
			// Add a point or a pair of points
			if (points.empty()) points.push_back(Point(tex, 0.5f, x, y));
			else {
				points.push_back(Point(tex, 0.0f, x, y - thickness));
				points.push_back(Point(tex, 1.0f, x, y + thickness));
			}
			oldval = val;
		}
		strip(points);
	}
	glColor3f(1.0, 1.0, 1.0);
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}
