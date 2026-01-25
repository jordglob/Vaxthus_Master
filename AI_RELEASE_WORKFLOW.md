# AI Assistant Release Workflow Guide

**Project:** Vaxthus_Master_V3 - ESP32 Grow Light Controller  
**Repository:** https://github.com/jordglob/Vaxthus_Master  
**Default Branch:** headless  
**Update Method:** Manual OTA via WiFi (NOT automatic GitHub updates)

---

## 🎯 IMPORTANT: Understand the Difference

### **OTA (Over-The-Air) Updates**
- **What:** Upload firmware to ESP32 via WiFi
- **Command:** `pio run -t upload --upload-port 192.168.38.112`
- **When:** After code changes, to test on device
- **Already implemented:** ✅ Working in v3.3.0+

### **GitHub Workflow**
- **What:** Version control and release management
- **Purpose:** Track versions, document changes, enable downloads
- **Does NOT:** Automatically update ESP32
- **This document covers:** GitHub workflow ONLY

**DO NOT mix these two concepts!**

---

## 📋 Complete Release Workflow

### **Phase 1: Development & Testing**

```bash
# 1. Ensure on correct branch
git checkout headless
git pull origin headless

# 2. Make code changes
# Edit src/main.cpp or other files

# 3. Test compilation
pio run

# 4. Upload to ESP32 for testing (via OTA)
pio run -t upload --upload-port 192.168.38.112
# OR use device hostname:
pio run -t upload --upload-port vaxthus-master.local

# 5. Monitor and verify
pio device monitor
# Press Ctrl+C to exit monitor
```

**Verify everything works before proceeding!**

---

### **Phase 2: Documentation Updates**

Update these files **in order**:

#### **1. src/main.cpp (if version tracking added later)**
```cpp
// Update version string if present
#define FIRMWARE_VERSION "3.X.0"
```

#### **2. README.md**
```markdown
# Update version badge (line 3)
![Version](https://img.shields.io/badge/version-3.X.0-brightgreen)

# Add new features to Features section (after line 11)
- **🆕 New Feature Name (v3.X.0)**: Description
- **📡 OTA Updates (v3.3.0)**: Upload via WiFi
- **🛡️ UV Safety Limiter (v3.2.0)**: Automatically limits UV
...

# Update serial monitor example if needed
```

#### **3. CHANGELOG.md**
```markdown
# Add new entry at TOP of file

## v3.X.0 - Feature Name (2026-XX-XX)

### Added
- New feature description
- Another addition

### Changed  
- Modified behavior X

### Fixed
- Bug fix Y

### Technical
- RAM: X.X% (XX,XXX bytes)
- Flash: XX.X% (XXX,XXX bytes)
```

#### **4. Other files if needed**
- INSTALL.md - if installation process changed
- BUILD.md - if build process changed
- FAQ.md - if new questions arise
- AI_PRIMER.md - if architecture changed

---

### **Phase 3: Git Commit & Tag**

```bash
# 1. Stage changed files
git add src/main.cpp README.md CHANGELOG.md
# Add other changed files as needed

# 2. Commit with semantic message
git commit -m "feat(v3.X): Add [feature name]

- Detailed description of changes
- What was added/modified
- RAM: X.X%, Flash: XX.X%"

# 3. Create version tag
git tag -a v3.X.0 -m "v3.X.0 - Feature Name

- Feature 1
- Feature 2
- Includes all previous features"

# 4. Push to GitHub
git push origin headless
git push --tags
```

**Tag naming convention:**
- **Major changes:** v4.0.0 (architecture changes)
- **New features:** v3.X.0 (adds functionality)
- **Bug fixes:** v3.3.X (patches)

---

### **Phase 4: GitHub Release (Web Interface)**

**Visit:** https://github.com/jordglob/Vaxthus_Master/releases

1. **Click:** "Draft a new release"

2. **Select tag:** v3.X.0 (from dropdown)

3. **Release title:** `v3.X.0 - Feature Name`

4. **Description template:**

```markdown
## 🚀 v3.X.0 - Feature Name

### New Features:
- ✅ **Your feature** - Description
- ✅ What it does
- ✅ Why it's useful

### How to Use:
[Instructions or examples]

### Technical Details:
- **RAM:** X.X% (XX,XXX bytes) - +X bytes from v3.(X-1)
- **Flash:** XX.X% (XXX,XXX bytes) - +XX bytes
- **Compilation:** SUCCESS ✅

### Includes All Previous Features:
- 📡 OTA Updates (v3.3.0)
- 🛡️ UV Safety Limiter (v3.2.0)  
- 🌅 Manual Mode Exit (v3.2.0)
- 🌅 Sun Simulation (v3.0.0)
- [List other features]

### Installation:

**From Source:**
```bash
git clone --branch v3.X.0 https://github.com/jordglob/Vaxthus_Master.git
cd Vaxthus_Master
pio run -t upload
```

**Via OTA (from v3.3.0+):**
```bash
# If already running v3.3.0 or later
pio run -t upload --upload-port 192.168.38.112
```

### Breaking Changes:
[If any - otherwise write "None"]

### Migration Guide:
[If needed - how to upgrade from previous version]
```

5. **Set as latest:** ☑️ Check "Set as the latest release"

6. **Click:** "Publish release"

---

### **Phase 5: Verification**

```bash
# 1. Check GitHub shows new version
# Visit: https://github.com/jordglob/Vaxthus_Master
# Homepage should show v3.X.0

# 2. Verify release page
# Visit: https://github.com/jordglob/Vaxthus_Master/releases
# Should show v3.X.0 as Latest

# 3. Test download
git clone --branch v3.X.0 https://github.com/jordglob/Vaxthus_Master.git test_download
cd test_download
pio run  # Should compile successfully
cd ..
rm -rf test_download

# 4. Check tags
git tag
# Should list v3.X.0

# 5. Verify on device
# ESP32 should still be running, check serial monitor or web interface
```

---

## 📝 File Update Checklist

For every release, update:

```
- [ ] src/main.cpp (if version string exists)
- [ ] README.md (version badge + features list)
- [ ] CHANGELOG.md (new entry at top)
- [ ] Git commit with semantic message
- [ ] Git tag: v3.X.0
- [ ] Push to headless branch
- [ ] Push tags
- [ ] Create GitHub release
- [ ] Verify release visible
- [ ] Test download works
```

---

## 🎨 Semantic Commit Messages

**Format:** `type(scope): description`

**Types:**
- `feat(vX.X)` - New feature
- `fix(vX.X)` - Bug fix
- `docs` - Documentation only
- `refactor` - Code refactoring
- `perf` - Performance improvement
- `test` - Add/modify tests
- `chore` - Maintenance

**Examples:**
```bash
git commit -m "feat(v3.4): Add automatic timezone detection"
git commit -m "fix(v3.3): Resolve OTA password bug"
git commit -m "docs: Update README with new features"
git commit -m "refactor: Simplify sun simulation logic"
```

---

## 🌿 Branch Strategy

**Primary branch:** `headless` (default on GitHub)

**Other branches:**
- `main` - Old v3.0.0 code (different architecture, keep for reference)
- `master` - Mirror of headless
- Feature branches - For experimental work

**Always work on:** `headless`

**Never merge to main** (it's a different codebase)

---

## 🔢 Version Numbering

**Format:** vMAJOR.MINOR.PATCH

**MAJOR (v4.0.0):**
- Complete rewrite
- Breaking changes
- Architecture change

**MINOR (v3.X.0):**
- New features
- New capabilities
- Backward compatible

**PATCH (v3.3.X):**
- Bug fixes
- Small improvements
- No new features

**Current:** v3.3.0
**Next minor:** v3.4.0
**Next major:** v4.0.0

---

## 📊 Memory Tracking

Always document RAM and Flash usage:

**Get from compile output:**
```
RAM:   [==        ]  15.3% (used 49972 bytes from 327680 bytes)
Flash: [=======   ]  66.9% (used 876401 bytes from 1310720 bytes)
```

**Add to CHANGELOG:**
```markdown
### Technical
- RAM: 15.3% (49,972 bytes) - +4,104 from v3.2
- Flash: 66.9% (876,401 bytes) - +44,376 from v3.2
```

**Track growth:** Each version should note memory change

---

## 🚀 OTA Update Instructions (for users)

**First Upload (via USB):**
```bash
pio run -t upload
```

**All Subsequent Uploads (via WiFi):**
```bash
pio run -t upload --upload-port 192.168.38.112
# OR
pio run -t upload --upload-port vaxthus-master.local
```

**Features:**
- Password protected (123456789)
- Automatic light shutoff during update
- Progress monitoring
- Safe rollback on failure

---

## 🤖 AI Assistant Guidelines

### **When user requests a new version:**

1. **Ask:** "What new feature should v3.X add?"

2. **Develop & test:** Code changes, compile, OTA upload

3. **Update docs:** README, CHANGELOG, others as needed

4. **Show changes:** Display what was modified

5. **Commit & tag:** Use semantic commit message

6. **Push:** Both code and tags

7. **Guide release:** Provide release description text

8. **Verify:** Check GitHub shows new version

### **Never:**
- ❌ Skip documentation updates
- ❌ Forget to update version badge
- ❌ Push without testing
- ❌ Create release without tag
- ❌ Mix OTA and GitHub workflows
- ❌ Work on wrong branch

### **Always:**
- ✅ Test code before committing
- ✅ Update all relevant docs
- ✅ Use semantic commit messages
- ✅ Create annotated tags
- ✅ Provide complete release notes
- ✅ Track memory usage
- ✅ Verify changes on GitHub

---

## 📂 Project Structure

```
Vaxthus_Master_V3/
├── src/
│   └── main.cpp           # Main firmware code
├── README.md              # Project overview
├── CHANGELOG.md           # Version history
├── INSTALL.md             # Installation guide
├── BUILD.md               # Build instructions
├── FAQ.md                 # Common questions
├── AI_PRIMER.md           # Architecture guide
├── AI_RELEASE_WORKFLOW.md # This file
├── platformio.ini         # Build configuration
└── .gitignore            # Git ignore rules
```

**Update priority:**
1. **Must update:** src/main.cpp, README.md, CHANGELOG.md
2. **Often update:** INSTALL.md (if process changed)
3. **Rarely update:** BUILD.md, FAQ.md, AI_PRIMER.md

---

## 🎯 Example: Creating v3.4.0

**User says:** "Add automatic timezone detection for v3.4"

**AI Response:**

```
Great! I'll add automatic timezone detection for v3.4.0.

Let me:
1. Add timezone detection code to src/main.cpp
2. Test compilation
3. Guide you through OTA upload
4. Update README and CHANGELOG
5. Create commit and tag
6. Provide release notes

Ready to start?
```

**Then follow this workflow document step by step.**

---

## 📞 Quick Reference

**Branch:** headless  
**Test:** `pio run -t upload --upload-port 192.168.38.112`  
**Commit:** `git commit -m "feat(vX.X): Description"`  
**Tag:** `git tag -a vX.X.0 -m "Message"`  
**Push:** `git push origin headless --tags`  
**Release:** https://github.com/jordglob/Vaxthus_Master/releases  

---

## ✅ Success Criteria

A release is complete when:
- ✅ Code compiles without errors
- ✅ Works on ESP32 (tested via OTA)
- ✅ README version badge updated
- ✅ CHANGELOG entry added
- ✅ Git tagged with vX.X.0
- ✅ Pushed to GitHub
- ✅ Release created on GitHub
- ✅ Downloadable and installable

---

**Remember:** This workflow ensures clean version separation, complete documentation, and professional releases. Follow it for every version update!
