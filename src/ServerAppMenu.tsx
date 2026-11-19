import {
  Description,
  NoteAddOutlined,
  OpenInBrowser,
  Save
} from "@mui/icons-material";
import {
  Divider,
  Drawer,
  IconButton,
  List,
  ListItemButton,
  ListItemIcon,
  ListItemText,
  Tooltip
} from "@mui/material";
import MenuIcon from "@mui/icons-material/Menu";
import styles from "./ServerAppMenu.module.css";

interface Props {
  open: boolean;
  projectName: string;
  dirty: boolean;
  onClose: () => void;
  onOpenProject: () => void;
  onSaveProject: () => void;
  onNewProject: () => void;
}

export default function ServerAppMenu({
  open,
  projectName,
  dirty,
  onClose,
  onOpenProject,
  onSaveProject,
  onNewProject
}: Props) {
  function run(action: () => void) {
    onClose();
    action();
  }

  return (
    <Drawer anchor="left" open={open} onClose={onClose}>
      <div className={styles.drawer}>
        <header className={styles.header}>
          <Tooltip title="Close main menu">
            <IconButton aria-label="Close main menu" onClick={onClose}>
              <MenuIcon />
            </IconButton>
          </Tooltip>
          <strong>Choreo</strong>
        </header>
        <div className={styles.projectName}>{projectName}</div>
        <List disablePadding>
          <ListItemButton onClick={() => run(onOpenProject)}>
            <ListItemIcon>
              <OpenInBrowser />
            </ListItemIcon>
            <ListItemText primary="Open Project" />
          </ListItemButton>
          <ListItemButton onClick={() => run(onSaveProject)}>
            <ListItemIcon>
              <Save />
            </ListItemIcon>
            <ListItemText primary="Save Project" />
          </ListItemButton>
          <ListItemButton onClick={() => run(onNewProject)}>
            <ListItemIcon>
              <NoteAddOutlined />
            </ListItemIcon>
            <ListItemText primary="New Project" />
          </ListItemButton>
          <Divider />
          <div className={styles.status}>
            <Description fontSize="small" />
            {dirty ? "Project has unsaved changes" : "All changes saved"}
          </div>
        </List>
      </div>
    </Drawer>
  );
}
