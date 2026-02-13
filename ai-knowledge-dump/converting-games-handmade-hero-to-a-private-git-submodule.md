# Converting `games/handmade-hero` to a Private Git Submodule

## Overview

**Current situation:**

- Main repo: `DreamEcho100/handmade-hero` (PUBLIC) - contains engine + game
- Goal: Extract `games/handmade-hero/` to a PRIVATE repo, link it back as a submodule

**After conversion:**

- `DreamEcho100/handmade-hero` (PUBLIC) - engine only, game linked as submodule
- `DreamEcho100/handmade-hero-game` (PRIVATE) - the actual game code

---

## Step 1: Create the Private Game Repository

```bash
# 1. Copy the game directory somewhere temporary
cp -r games/handmade-hero /tmp/handmade-hero-game
cd /tmp/handmade-hero-game

# 2. Initialize as new git repo (fresh history)
rm -rf .git 2>/dev/null  # Remove any nested git if present
git init
git add .
git commit -m "Initial commit: handmade-hero game"

# 3. Create private repo on GitHub and push
gh repo create DreamEcho100/handmade-hero-game --private --source=. --push

# Or manually:
# - Create private repo on GitHub: DreamEcho100/handmade-hero-game
# - Then:
git remote add origin git@github.com:DreamEcho100/handmade-hero-game.git
git push -u origin main
```

---

## Step 2: Remove Directory and Add as Submodule

```bash
# Back in the main repo
cd /home/viavi/Desktop/workspaces/github/DreamEcho100/handmade-hero

# 1. Remove the game directory from git tracking
git rm -r games/handmade-hero

# 2. Commit the removal
git commit -m "chore: remove handmade-hero game (moving to private submodule)"

# 3. Add as submodule
git submodule add git@github.com:DreamEcho100/handmade-hero-game.git games/handmade-hero

# 4. Commit and push
git commit -m "chore: add handmade-hero as private submodule"
git push
```

---

## Step 3: Daily Workflow

### Making Changes to the Game

```bash
# 1. Navigate to submodule
cd games/handmade-hero

# 2. Make changes, commit, push
git add .
git commit -m "feat: your changes"
git push origin main

# 3. Update main repo's reference
cd ../..
git add games/handmade-hero
git commit -m "chore: update handmade-hero submodule"
git push
```

### Cloning the Project (Fresh Machine)

```bash
# Clone with submodule
git clone --recursive git@github.com:DreamEcho100/handmade-hero.git

# Or if already cloned without --recursive:
git submodule update --init --recursive
```

### Pulling Updates

```bash
git pull
git submodule update --recursive
```

---

## Step 4: Useful Git Aliases (Optional)

Add to `~/.gitconfig`:

```ini
[alias]
    # Pull main + update submodules
    pull-all = !git pull && git submodule update --recursive

    # Clone with submodules
    clone-full = clone --recursive
```

---

## Summary Checklist

- [ ] Copy `games/handmade-hero` to temp location
- [ ] Init new git repo, create private `DreamEcho100/handmade-hero-game`
- [ ] Remove `games/handmade-hero` from main repo
- [ ] Add private repo as submodule at `games/handmade-hero`
- [ ] Verify build still works
- [ ] Push both repos

```

```
